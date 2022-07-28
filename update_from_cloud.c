/**
 * File              : update_from_cloud.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 29.07.2022
 * Last Modified Date: 29.07.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "sqlite2yandexdisk.h"
#include "cYandexDisk/cJSON.h"
#include "cYandexDisk/cYandexDisk.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STR(...)\
	({char ___str[BUFSIZ]; snprintf(___str, BUFSIZ-1, __VA_ARGS__); ___str[BUFSIZ-1] = 0; ___str;})

struct update_from_cloud_t {
	void * user_data;
	int (*callback)(size_t size, void *user_data, char *error);			
	time_t timestamp;
	char tablename[256];
	char uuid[37];
	char database[BUFSIZ];
};

struct columns_list_t {
	struct columns_list_t *prev;
	char column_name[256];
};

struct columns_list_t * new_columns_list(){
	struct columns_list_t * list = malloc(sizeof(struct columns_list_t));
	if (list == NULL){
		perror("columns_list_t malloc");
		return NULL;
	}
	list->prev = NULL;
	list->column_name[0] = 0;

	return list;
}

int columns_list_callback(void *user_data, int argc, char **argv, char **titles){
	struct columns_list_t ** list = user_data;
	int i;
	for (i = 0; i < argc; ++i) {
		char * column_name = titles[i];
		if (column_name == NULL)
			column_name = "";

		if (strcmp(column_name, "uuid") != 0){ //don't add uuid column
			//new list node
			struct columns_list_t * new_list = new_columns_list();
			new_list->prev = *list;
			strncpy(new_list->column_name, column_name, 255);
			new_list->column_name[255] = 0;
			*list = new_list;
		}
	}

	return 1; //stop execution
}

int update_from_cloud_callback(size_t size, void *data, void *user_data, char *error){			
	struct update_from_cloud_t *t = user_data;
	
	if (error)
		if(t->callback)
			t->callback(0, t->user_data, STR("%s", error));
	

	if (size){
		//const char *err;
		//cJSON * json = cJSON_ParseWithLengthOpts((const char *)data, size, &err, 0);
		
		//if (err)
			//if(t->callback)
				//t->callback(0, t->user_data, STR("cJSON_Parse error: %s", err));		
		
		cJSON * json = cJSON_Parse(data);

		printf("JSON: %s\n", cJSON_Print(json));

		//check json
		if (!cJSON_IsObject(json)){
			if (t->callback)
				t->callback(0, t->user_data, STR("can't get json from timestamp: %ld for %s: %s", t->timestamp, t->tablename, t->uuid));
			return 1;
		}

		//get list of SQLite table columns
		struct columns_list_t * list = new_columns_list();
		{
			char SQL[BUFSIZ];
			sprintf(SQL, "SELECT * FROM %s", t->tablename);
			sqlite_connect_execute_function(SQL, t->database, &list, columns_list_callback);
		}

		//for each node in list
		while(list->prev != NULL) {
			cJSON * item  = cJSON_GetObjectItem(json, list->column_name);
			char  * value = cJSON_GetStringValue(item);
			printf("VALUE: %s\n", cJSON_Print(item));

			if (value){
				size_t size = strlen(value);
				if (size > 0) {
					char * SQL  = malloc(BUFSIZ + size);	
					if (SQL == NULL){
						perror("SQL malloc");
					}
					snprintf(SQL,
							BUFSIZ + size,
							"INSERT INTO %s (uuid) "
							"SELECT '%s' "
							"WHERE NOT EXISTS (SELECT 1 FROM %s WHERE uuid = '%s'); "
							"UPDATE %s SET '%s' = '%s' WHERE uuid = '%s'"
							,
							t->tablename, 
							t->uuid,
							t->tablename, t->uuid,
							t->tablename, list->column_name, value, t->uuid		
					);
					printf("SQL STRING: %s\n", SQL);
					sqlite_connect_execute(SQL, t->database);
					free(SQL);
				}

			}
			//do cicle
			struct columns_list_t * ptr = list;
			list = list->prev;
			free(ptr);
		}
		free(list);

		//delete JSON
		cJSON_Delete(json);

		//free user_data
		if (user_data)
			free(user_data);
	}

	return 0;
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

struct timestamps_callback_t{
	void * user_data;
	struct timestamp_array * timestamps;
	int (*callback)(size_t size, void *user_data, char *error);			
};

int timestamps_callback(c_yd_file_t *file, void *user_data, char *error){
	struct timestamps_callback_t * t = user_data;
	if (error)
		if(t->callback)
			t->callback(0, t->user_data, STR("%s", error));
	
	time_t timestamp = atol(file->name);
	timestamp_array_append(t->timestamps, timestamp);

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
	//get list of timestamps
	char rowpath[BUFSIZ];
	sprintf(rowpath, "app:/%s/%s/%s", path, tablename, uuid);

	struct timestamp_array timestamps;
	timestamp_array_init(&timestamps);
	struct timestamps_callback_t t = {
		.timestamps = &timestamps,
		.user_data = user_data,
		.callback = callback
	};

	c_yandex_disk_ls(token, rowpath, &t, timestamps_callback);

	//check list - if timestamps differ
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
	struct update_from_cloud_t * d = malloc(sizeof(struct update_from_cloud_t));
	if (d == NULL){
		perror("update_from_cloud_t malloc");
		if (callback)
			callback(0, user_data, "update_from_cloud_t malloc");
		
		return;
	}

	d->callback = callback;
	d->user_data = user_data;
	d->timestamp = max;
	strcpy(d->database,database);
	strcpy(d->tablename, tablename);
	strcpy(d->uuid, uuid);
	
	char key[BUFSIZ]; 
	sprintf(key, "%ld", max);

	char keypath[BUFSIZ];
	sprintf(keypath, "app:/%s/%s/%s/%s", path, tablename, uuid, key);	

	printf("YandexDisk dowload path: %s, token: %s\n", keypath, token);

	c_yandex_disk_download_data(
			token,	
			keypath, 
			true,
			d, 
			update_from_cloud_callback, 
			NULL, 
			NULL
			);
	
}
