/**
 * File              : sqlite2yandexdisk.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 03.05.2022
 * Last Modified Date: 26.07.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sqlite2yandexdisk.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/cJSON.h"
#include "cYandexDisk/cYandexDisk.h"
#include "cYandexDisk/klib/alloc.h"

#define STR(...)\
	({char ___str[BUFSIZ]; snprintf(___str, BUFSIZ-1, __VA_ARGS__); ___str[BUFSIZ-1] = 0; ___str;})

int
sqlite2yandexdisk_create_directories(
		const char * token,
		const char * path,
		const char * tablename
		)
{
	//buffer for path
	char buf[BUFSIZ];
	strncpy(buf, path, BUFSIZ - 1); buf[BUFSIZ - 1] = '\0';
	//make directory for path - strtok to use path components
	char _path[BUFSIZ] = "app:";
	printf("We have a path path: %s\n", buf);
	char *p = strtok(buf, "/");
	while (p) {
		char *error = NULL;
		sprintf(_path, "%s/%s", _path, p);
		printf("make directories for path: %s\n", _path);
		c_yandex_disk_mkdir(token, _path, &error);
		if (error) {
			perror(error);
		}
		p = strtok(NULL, "/");
	}
	
	//make directory for table
	char *error = NULL;
	char tablepath[BUFSIZ]; 
	sprintf(tablepath, "%s/%s", _path, tablename);
	printf("make directories for table: %s\n", tablepath);
	c_yandex_disk_mkdir(token, tablepath, &error);
	if (error) {
		perror(error);
	}	

	return 0;
}

struct upload_value_for_key_data {
	void * value;
	void *user_data;
	int (*callback)(size_t size, void *user_data, char *error);
};

int upload_value_for_key_callback(size_t size, void *user_data, char *error){
	//this is needed to free value and call callback
	struct upload_value_for_key_data * d = user_data;
	d->callback(size, d->user_data, error);

	if (!error) {
		free(d->value);
		free(d);
	}

	return 0;
}

void
upload_value_for_key(
		const char * token,
		const char * path,
		const char * tablename,
		const char * identifier,
		void * value,
		size_t size,
		const char * key,
		void *user_data,           //pointer of data to transfer throw callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of uploaded file
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		)
{
	sqlite2yandexdisk_create_directories(token, path, tablename);
	
	//create directory for identifier
	char rowpath[BUFSIZ];
	sprintf(rowpath, "app:/%s/%s/%s", path, tablename, identifier);
	char *error = NULL;
	c_yandex_disk_mkdir(token, rowpath, &error);
	if (error) {
		perror(error);
	}	

	//upload data for key
	struct upload_value_for_key_data * d = NEW(struct upload_value_for_key_data);
	d->value = value;
	d->user_data = user_data;
	d->callback = callback;

	char keypath[BUFSIZ];
	sprintf(keypath, "%s/%s", rowpath, key);
	c_yandex_disk_upload_data(
			token, 
			value, 
			size, 
			keypath, 
			true,
			false,
			d, 
			upload_value_for_key_callback, 
			NULL, 
			NULL
			);
}

void
sqlite2yandexdisk_download_value_for_key(
		const char * token,
		const char * path,
		const char * tablename,
		const char * identifier,
		const char * key,
		void *user_data,        //pointer of data return from callback
		int (*callback)(
			size_t size,        //size of downloaded data
			void *data,			//pointer of downloaded data
			void *user_data,    //pointer of data return from callback
			char *error			//error
			)
		)
{
	char keypath[BUFSIZ];
	sprintf(keypath, "app:/%s/%s/%s/%s", path, tablename, identifier, key);	

	printf("YandexDisk dowload path: %s, token: %s\n", keypath, token);

	c_yandex_disk_download_data(
			token,	
			keypath, 
			true,
			user_data, 
			callback, 
			NULL, 
			NULL
			);
}				

int sqlite2yandexdisk_sqlite2json_callback(void *data, int argc, char **argv, char **titles) {
	//create json
	cJSON * json = data;

	//for each column
	int i;
	for (i = 0; i < argc; ++i) {
		char * title = titles[i];
		char * value = argv[i]; if (value == NULL) value = "";

		cJSON_AddItemToObject(json, title, cJSON_CreateString(value));
	}

	return 1; //stop execution
}

void
sqlite2yandexdisk_upload(
		const char * token,
		const char * path,
		const char * database,
		const char * tablename,
		const char * uuid,		
		time_t timestamp,		   //last change of local data
		void *user_data,		   //pointer of data return from callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of uploaded file
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		)
{
	//get json from sqlite
	cJSON * json = cJSON_CreateObject();
	char SQL[BUFSIZ];
	sprintf(SQL, "SELECT * FROM %s WHERE uuid ='%s'", tablename, uuid);
	sqlite_connect_execute_function(SQL, database, json, sqlite2yandexdisk_sqlite2json_callback);

	//check json
	if (cJSON_GetArraySize(json) < 1){
		if (callback)
			callback(0, user_data, STR("can't create JSON for %s: %s", tablename, uuid));
		cJSON_Delete(json);
		return;
	}

	//save json to Yandex Disk
	char * value = cJSON_Print(json);
	size_t size = strlen(value);

	printf("JSON TO SAVE: %s\n", value);
	char key[16]; sprintf(key, "%ld", timestamp); //timestamp as key
	
	upload_value_for_key(token, path, tablename, uuid, value, size, key, user_data, callback);

	//delete JSON
	cJSON_Delete(json);
}

struct timestamp_array {
	time_t * data;
	int len;
};

void timestamp_array_init(struct timestamp_array * array){
	array->data = malloc(sizeof(time_t));
	if (array->data == NULL){
		perror("malloc timestamp_array");
		exit(EXIT_FAILURE);
	}
	array->len = 0;
}

void timestamp_array_append(struct timestamp_array * array, time_t item){
	array->data[array->len] = item;
	array->len++;
	array->data = realloc(array->data, sizeof(time_t) + array->len * sizeof(time_t));
	if (array->data == NULL){
		perror("malloc timestamp_array");
		exit(EXIT_FAILURE);
	}	
}

struct list_of_data_callback_data{
	void * user_data;
	struct timestamp_array * timestamps;
	int (*callback)(size_t size, void *user_data, char *error);			
};

int list_of_data_callback(c_yd_file_t *file, void *user_data, char *error){
	struct list_of_data_callback_data * d = user_data;
	if (error)
		if(d->callback)
			d->callback(0, d->user_data, STR("%s", error));
	
	time_t timestamp = atol(file->name);
	timestamp_array_append(d->timestamps, timestamp);

	return 0;
}

struct sqlite2yandexdisk_update_from_cloud_data {
	void * user_data;
	int (*callback)(size_t size, void *user_data, char *error);			
	time_t timestamp;
	char tablename[256];
	char uuid[37];
	char database[BUFSIZ];
};

int sqlite2yandexdisk_update_from_cloud_callback(size_t size, void *data, void *user_data, char *error){			
	printf("start sqlite2yandexdisk_update_from_cloud_callback\n");
	struct sqlite2yandexdisk_update_from_cloud_data *d = user_data;
	
	if (error)
		if(d->callback)
			d->callback(0, d->user_data, STR("%s", error));
	
	printf("SIZE: %ld\n", size);

	if (size){
		//const char *err;
		//cJSON * json = cJSON_ParseWithLengthOpts((const char *)data, size, &err, 0);
		
		//if (err)
			//if(d->callback)
				//d->callback(0, d->user_data, STR("cJSON_Parse error: %s", err));		
		cJSON * json = cJSON_Parse((const char *)data);
		
		printf("JSON: %s\n", cJSON_Print(json));

		//check json
		if (!cJSON_IsObject(json)){
			if (d->callback)
				d->callback(0, d->user_data, STR("can't get json from timestamp: %ld for %s: %s", d->timestamp, d->tablename, d->uuid));
			return 1;
		}


		cJSON * item  = cJSON_GetArrayItem(json, 0);
		
		char  * key   = item->string;
		char  * value = cJSON_GetStringValue(item);

		size_t size = strlen(value);
		char * SQL  = MALLOC(BUFSIZ + size);	
		snprintf(SQL,
				size,
				"INSERT INTO %s (uuid) "
				"SELECT '%s' "
				"WHERE NOT EXISTS (SELECT 1 FROM %s WHERE uuid = '%s'); "
				"UPDATE %s SET '%s' = '%s' WHERE uuid = '%s'"
				,
				d->tablename, 
				d->uuid,
				d->tablename, d->uuid,
				d->tablename, key, value, d->uuid		
		);
		sqlite_connect_execute(SQL, d->database);

		//delete JSON
		cJSON_Delete(json);

		//free user_data
		if (user_data)
			free(user_data);
	}

	return 0;
}
		
void
sqlite2yandexdisk_update_from_cloud(
		const char * token,
		const char * path,
		const char * database,
		const char * tablename,
		const char * uuid,		
		time_t timestamp,
		bool rebase_with_timestamp,
		void *user_data,		   //pointer of data return from callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of downloaded data
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		)
{
	//get list of saved data
	char rowpath[BUFSIZ];
	sprintf(rowpath, "app:/%s/%s/%s", path, tablename, uuid);

	struct timestamp_array timestamps;
	timestamp_array_init(&timestamps);
	struct list_of_data_callback_data t = {
		.timestamps = &timestamps,
		.user_data = user_data,
		.callback = callback
	};

	c_yandex_disk_ls(token, rowpath, &t, list_of_data_callback);

	//check list
	if (timestamps.len < 1) {
		if (callback)
			callback(0, user_data, STR("can't get list of saved data for %s: %s", tablename, uuid));
		free(timestamps.data);
		return;
	}

	//get max time
	time_t max = 0;

	if (rebase_with_timestamp) {
		max = timestamp;
		free(timestamps.data);
	} else {
		int i;
		for (i = 0; i < timestamps.len; ++i) {
			time_t _t = timestamps.data[i];	
			if (_t > max) max = _t;
		}
		free(timestamps.data);

		//check if need to update
		if (timestamp > max) {
			if (callback)
				callback(0, user_data, STR("no need to update: last change: %ld > yandex disk timestamp: %ld for %s: %s", timestamp, max, tablename, uuid));
			return;
		}
	}

	//download json for max time and update SQLite 
	struct sqlite2yandexdisk_update_from_cloud_data * d = NEW(struct sqlite2yandexdisk_update_from_cloud_data);
	d->callback = callback;
	d->user_data = user_data;
	d->timestamp = max;
	strcpy(d->database,database);
	strcpy(d->tablename, tablename);
	strcpy(d->uuid, uuid);
	char key[BUFSIZ]; sprintf(key, "%ld", max);
	sqlite2yandexdisk_download_value_for_key(token, path, tablename, uuid, key, d, sqlite2yandexdisk_update_from_cloud_callback);
}
