#include "download.h"
#include <curl/curl.h>

namespace pullit {

Retriever::Retriever() {
    curl_global_init(CURL_GLOBAL_ALL);
}

Retriever::~Retriever() {
    cout << "Retriever: Cleaning up.";
    curl_global_cleanup();
}

Retriever &Retriever::inst() {
    static Retriever INST;
    return INST;
}

size_t Retriever::write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    
    cout << ".";

    return written;
}

bool Retriever::download(string url, string path) {
    bool success = false;
    CURL *curl_handle = curl_easy_init();

    cout << "Downloading file " << url << " ..." << endl;

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    FILE *pagefile = fopen(path.c_str(), "wb");

    if(pagefile) {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
        curl_easy_perform(curl_handle);
        fclose(pagefile);

        success = true;
    }

    cout << endl;

    curl_easy_cleanup(curl_handle);

    return success;
}

        
}
