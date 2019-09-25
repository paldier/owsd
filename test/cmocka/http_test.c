#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <curl/curl.h>

#include <stdbool.h>

#define URL "127.0.0.1"
#define PATH "index.html"
#define WWW_PATH "/www/index.html"


static bool file_compare(char *path1, char *path2)
{
 	FILE *fp1, *fp2;
	char ch1, ch2;
	bool equal = true;

	fp1 = fopen(path1, "r");
 	fp2 = fopen(path2, "r");


	while ((ch1 = getc(fp1)) && ch1 != EOF && (ch2 = getc(fp2)) && ch2 != EOF) {
		if (ch1 != ch2) {
			equal = false;
			break;
		}
	}

	fclose(fp1);
	fclose(fp2);

	return equal;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}

static void test_get_url(void **state)
{
	CURL *curl_handle;
	static const char *pagefilename = "index.html";
	FILE *pagefile;

	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */
	curl_handle = curl_easy_init();

	/* set URL to get here */
	curl_easy_setopt(curl_handle, CURLOPT_URL, URL);

	/* Switch on full protocol/debug output while testing */
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

	/* disable progress meter, set to 0L to enable it */
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

	/* send all data to this function  */
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

	/* open the file */
	pagefile = fopen(pagefilename, "wb");
	if (pagefile) {
		/* write the page body to this file handle */
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);

		/* get it! */
		curl_easy_perform(curl_handle);

		/* close the header file */
		fclose(pagefile);
	}

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	curl_global_cleanup();
	assert_true(file_compare("./index.html", "/www/index.html"));
}

static int clean_path(void **state)
{
	(void) state;

	remove("./index.html");

	return 0;
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_get_url, clean_path, clean_path),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
