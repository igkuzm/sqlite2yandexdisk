/**
 * File              : upload.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 03.05.2022
 * Last Modified Date: 29.07.2022
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

#define STR(...)\
	({char ___str[BUFSIZ]; snprintf(___str, BUFSIZ-1, __VA_ARGS__); ___str[BUFSIZ-1] = 0; ___str;})

int
create_directories(
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

struct upload_value_for_key_t {
	void * value;
	void *user_data;
	int (*callback)(size_t size, void *user_data, char *error);
};

int upload_value_for_key_callback(size_t size, void *user_data, char *error){
	//this is needed to free value and call callback
	struct upload_value_for_key_t * t = user_data;
	t->callback(size, t->user_data, error);

	if (!error) {
		free(t->value);
		free(t);
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
	create_directories(token, path, tablename);
	
	//create directory for identifier
	char rowpath[BUFSIZ];
	sprintf(rowpath, "app:/%s/%s/%s", path, tablename, identifier);
	char *error = NULL;
	c_yandex_disk_mkdir(token, rowpath, &error);
	if (error) {
		perror(error);
	}	

	//upload data for key
	struct upload_value_for_key_t * t = malloc(sizeof(struct upload_value_for_key_t));
	if (t==NULL){
		perror("upload_value_for_key_t malloc");
		if (callback)
			callback(0, user_data, "Can't allocate memory for upload_value_for_key_t");
		return;
	}
	t->value = value;
	t->user_data = user_data;
	t->callback = callback;

	char keypath[BUFSIZ];
	sprintf(keypath, "%s/%s", rowpath, key);
	c_yandex_disk_upload_data(
			token, 
			value, 
			size, 
			keypath, 
			true,
			false,
			t, 
			upload_value_for_key_callback, 
			NULL, 
			NULL
			);
}


int sqlite2json_callback(void *data, int argc, char **argv, char **titles) {
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
	sqlite_connect_execute_function(SQL, database, json, sqlite2json_callback);

	//check json
	if (cJSON_GetArraySize(json) < 1){
		if (callback)
			callback(0, user_data, STR("can't create JSON for %s: %s", tablename, uuid));
		cJSON_Delete(json);
		return;
	}

	//save json to Yandex Disk
	char * value = cJSON_Print(json);
	printf("JSON TO UPLOAD: %s\n", value);
	size_t size = strlen(value);
	printf("JSON SIZE: %ld\n", size);

	char key[16]; sprintf(key, "%ld", timestamp); //timestamp as key
	
	upload_value_for_key(token, path, tablename, uuid, value, size, key, user_data, callback);

	//delete JSON
	cJSON_Delete(json);
}
