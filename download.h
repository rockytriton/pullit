#include "common.h"

namespace pullit {

class Retriever {
public:
    static Retriever &inst();
    ~Retriever();

    bool download(string url, string path);

private:
    Retriever();

    static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);
};

}