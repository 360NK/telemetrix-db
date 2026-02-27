#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <stdint.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("Not Enough Memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

uint8_t* fetch_gtfs_data(const char* url, size_t* out_size) {
    printf("[Fetcher] thread started. Ready to hit the network. \n");

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        free(chunk.memory);
        return NULL;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    if(res != CURLE_OK) {
        fprintf(stderr, "[Fetcher] Download Failed: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        return NULL;
    }

    *out_size = chunk.size;
    return (uint8_t *)chunk.memory;

}