/**
 * File              : sqlite2yandexdisk.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 03.05.2022
 * Last Modified Date: 19.07.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void
sqlite2yandexdisk_upload(
		const char * token,        //yandex disk auth token
		const char * path,         //yandex disk remote data path
		const char * database,     //SQLite database path
		const char * tablename,    //name of table
		const char * uuid,		   //uuid of row in table	
		time_t timestamp,		   //last change of local data
		void *user_data,		   //pointer of data return from callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of uploaded file
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		);

void
sqlite2yandexdisk_update_from_cloud(
		const char * token,		   //Yandex Disk auth token
		const char * path,		   //path for remote data
		const char * database,     //path for SSQLite database
		const char * tablename,    //name of table
		const char * uuid,		   //identifier of row in table	
		time_t timestamp,		   //last change of local data or remote data timestamp for rebase
		bool rebase_with_timestamp,//rebase local data with remote data
		void *user_data,		   //pointer of data return from callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of downloaded data
			void * user_data,      //pointer of data return from callback
			char * error		   //error
			)		
		);

#ifdef __cplusplus
}  /* end of the 'extern "C"' block */
#endif

