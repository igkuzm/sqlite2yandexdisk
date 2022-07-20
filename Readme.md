# C API for sync SQLite3 database to YandexDisk

```
int callback(size_t size, void * user_data, char * error)
{
	if (error)
		fprintf(stderr, "ERROR: %s\n", error);
	if (size > 0)
		fprintf(stderr, "Data transfer complete!\n");
	
	return 0;
}

int up_progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	fprintf(stderr, "UPLOADED: %f%%\n", ulnow/ultotal*100);
	return 0;
}

int dl_progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	fprintf(stderr, "DOWNLOADED: %f%%\n", dlnow/dltotal*100);
	return 0;
}

int main(int argc, char *argv[])
{
	printf("cYandexDisk Test\n");
	
	printf("Upload file\n");
	c_yandex_disk_upload_file("AUTH TOKEN FROM YANDEX", "~/Desktop/test.jpeg", "app:/test.jpeg", NULL, callback, NULL, up_progress_callback);

	printf("Download file\n");
	c_yandex_disk_download_file("AUTH TOKEN FROM YANDEX", "~/Desktop/test_downloaded.jpeg", "app:/test.jpeg", NULL, callback, NULL, dl_progress_callback);

	return 0;
}

```
