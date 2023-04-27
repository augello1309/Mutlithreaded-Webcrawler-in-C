// * Alex Augello, Altin Ame, Vlady Salazar *
// * Webcrawler Project for Operating Systems *
//


// Headers used
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define URLLEN 1000 // max length of the URL
pthread_mutex_t lock; // defining mutex lock

int max_connections = 200; // maximum amount of connections to be established
int max_total = 100; // maximum ammount of URLS to be crawled
int max_requests = 500; //
int max_link_per_page = 5; // can only pick up 5 URLs per page
int followlinks = 0; // test variable

int pending_interrupt = 0; // define interrupt variable as 0

void sighandler(int dummy)
{
  pending_interrupt = 1; // 1 indicates interrupt
}

// Define the buffer to hold HTML Links
typedef struct {
  char *buf;
  size_t size;
} memory;

// Memory allocation for buffer, stores links to be crawled
size_t grow_buffer(void *contents, size_t sz, size_t nmemb, void *ctx)
{
  size_t realsize = sz * nmemb;
  memory *mem = (memory*) ctx;
  char *ptr = realloc(mem->buf, mem->size + realsize);
  if(!ptr) {
    // Machine is out of memory
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  mem->buf = ptr;
  memcpy(&(mem->buf[mem->size]), contents, realsize);
  mem->size += realsize;
  return realsize;
}

// CURL FUNCTION to make handle
CURL *make_handle(char *url)
{
  CURL *handle = curl_easy_init(); // initalize curl

  // HTTP2 allows for multiplexing over HTTPS
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
  curl_easy_setopt(handle, CURLOPT_URL, url);

  // buffer
  memory *mem = malloc(sizeof(memory)); // allocate memory to mem structure
  mem->size = 0;
  mem->buf = malloc(1);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, grow_buffer);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, mem);
  curl_easy_setopt(handle, CURLOPT_PRIVATE, mem);

  // CURL documentation recommended to include these to prevent errors
  curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, ""); // Specifies what encoding we want... "" supports ALL Encodings
  curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L); // how long we allow the libcurl transfer operation to take
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L); // follow HTTP redirects
  curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L); // defines maximum amount of redirects
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 2L); // timeout for the entire request
  curl_easy_setopt(handle, CURLOPT_COOKIEFILE, ""); // holds cookies
  curl_easy_setopt(handle, CURLOPT_FILETIME, 1L); // gets the modification down of the remote resource
  curl_easy_setopt(handle, CURLOPT_USERAGENT, "Crawler Project"); // sets header in the HTTP request sent to remote server
  curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY); // authentication methods we want to use when speaking to remote server
  curl_easy_setopt(handle, CURLOPT_UNRESTRICTED_AUTH, 1L); // make CURL trust the server
  curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY); // authentication methods we want to use when speaking to remote server
  curl_easy_setopt(handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L); // tell libcurl the number of milliseconds to wait for a server response
  return handle;
}

// HTML Parser using libxml2
size_t follow_links(CURLM *multi_handle, memory *mem, char *url)
{
  int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
             HTML_PARSE_NOWARNING | HTML_PARSE_NONET; // Parser Options: removes all blank nodes, suppresses error and warnings, forbids network access
  htmlDocPtr doc = htmlReadMemory(mem->buf, mem->size, url, NULL, opts);
  if(!doc)
    return 0; // exit out of function if doc is NULL

  xmlChar *xpath = (xmlChar*) "//a/@href"; // in HTML links are followed by hrefs
  xmlXPathContextPtr context = xmlXPathNewContext(doc); // initalize context pointer
  xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context); // initalize object pointer
  xmlXPathFreeContext(context); // free context
  if(!result)
    return 0; // exit out of function if obj ptr is NULL

  xmlNodeSetPtr nodeset = result->nodesetval;
  if(xmlXPathNodeSetIsEmpty(nodeset)) { // check for empty nodeset
    xmlXPathFreeObject(result); // free memory in result
    return 0;
  }
  size_t count = 0;
  int i;
  for(i = 0; i < nodeset->nodeNr; i++) {
    double r = rand();
    int x = r * nodeset->nodeNr / RAND_MAX; // random node to follow
    const xmlNode *node = nodeset->nodeTab[x]->xmlChildrenNode;
    xmlChar *href = xmlNodeListGetString(doc, node, 1); // defining href variable
    if(followlinks) {
      xmlChar *orig = href;
      href = xmlBuildURI(href, (xmlChar *) url); // builds URL and store it into href variable
      xmlFree(orig); // free memory
    }
    char *link = (char *) href;
    if(!link || strlen(link) < 20)
      continue;
    if(!strncmp(link, "http://", 7) || !strncmp(link, "https://", 8)) { // check for http:// or https:// links -- we can use both
      curl_multi_add_handle(multi_handle, make_handle(link));
      if(count++ == max_link_per_page)
        break;
    }
    xmlFree(link);
  }
  xmlXPathFreeObject(result);
  return count;
}




// checks if html page
int html_checker(char *ctype)
{
  return ctype != NULL && strlen(ctype) > 10 && strstr(ctype, "text/html");
}

int crawler(char *start_page)
{
    pthread_mutex_lock(&lock); //lock thread to avoid race conditions
    FILE *datafile = fopen("datafile.txt", "a");
    signal(SIGINT, sighandler); //sends a signal
    LIBXML_TEST_VERSION; // safety check to make sure the libxml.dll is compatible with headers
    curl_global_init(CURL_GLOBAL_DEFAULT); //initialize curl
    CURLM *multi_handle = curl_multi_init(); //initalize curl multiplexing
    curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, max_connections); //handles the max multiplexing connections
    curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 6L); //Also handles the multiplexing

  // Enables http/2 access if its possible
#ifdef CURLPIPE_MULTIPLEX
  curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX); // CURL
#endif

  //Sets html start page
  curl_multi_add_handle(multi_handle, make_handle(start_page));

  int msgs_left; //messages left to send
  int pending = 0; //pending URLs to crawl
  int complete = 0; //complete URLs crawled
  int still_running = 1; //flag to check if program needs to continue crawling
  while(still_running && !pending_interrupt) {
    int numfds; //Checks for file descriptors to check for
    curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
    curl_multi_perform(multi_handle, &still_running); //tells the program whether or not to continue crawling

    // Check status of transfers
    CURLMsg *m = NULL; //performs data transfers on all added handles
    while((m = curl_multi_info_read(multi_handle, &msgs_left))) { //while there are messages
      if(m->msg == CURLMSG_DONE) { //Identifies if message transfer is completed
        CURL *handle = m->easy_handle; //stores the
        char *url; //url to be crawled
        memory *mem;
        curl_easy_getinfo(handle, CURLINFO_PRIVATE, &mem); // request information from private pointer (mem)
        curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url); // request information from URL
        if(m->data.result == CURLE_OK) {
          long res_status;
          curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &res_status);
          if(res_status == 200) { // 200 status means successful connection
            char *ctype;
            curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ctype);
            fprintf(datafile, "[%d]: %s\n",complete, url); // print successful crawl to text file
            if(html_checker(ctype) && mem->size > 100) {
              if(pending < max_requests && (complete + pending) < max_total) { // pending links to crawl is less than max_pending,
                                                                              // successfully crawled links + links left to crawl is less than total amt of links to crawl
                pending += follow_links(multi_handle, mem, url); // follow_links returns a counter of links to follow, we update pending with this information
                still_running = 1;
              }
            }
          }
          else {
            fprintf(datafile, "[%d]: %s\n",complete, url); // print even if res_status is not 200
          }
        }
        else {        // else CURLE_OK did not return 0, failed to connect
          printf("[%d] Connection failure: %s\n", complete, url); // print failure message to terminal
        }
        //free the memory
        curl_multi_remove_handle(multi_handle, handle);
        curl_easy_cleanup(handle);
        free(mem->buf);
        free(mem);
        complete++; // increment variable, amount of successfully crawled links
        pending--; // decrement pending, amount of links left to crawl
      }
    }
  }
  fclose(datafile); // close file
  curl_multi_cleanup(multi_handle);
  curl_global_cleanup(); // clean up memory and end curl
  pthread_mutex_unlock(&lock); //unlock thread to allow next thread to continue
  return 0;

}

// main function
int main(void)
{
    pthread_mutex_init(&lock, NULL); //initialize mutex lock
    FILE *datafile = fopen("datafile.txt", "w"); // store links in text file
    int j = 0;
    char filename[50];
    printf("Which file would you like to open?: "); // ask user what file they want to open
    scanf("%s", filename);
    int seedlinksnum = 0; // amount of links in the file
    FILE *fp;
    fp = fopen(filename, "r"); // open file for reading
    while(!fp)
    {
        fprintf(stderr, "Error: failed to open file.\n");
        printf("Enter file name again or exit(Ctrl+Z): ");
        scanf("%s", filename);
        fp = fopen(filename, "r");
    }


    char *urls[10]; // initalize pointer array of URLS
    for(j = 0; j < 10; j++)
    {
        urls[j] = malloc(URLLEN); // allocate memory for the URLS
        if(urls[j] == NULL) // error
            fprintf(stderr, "Error allocating memory");

    }
    char urlbuffer[1000]; // buffer for URLs

    //gets each line from file and stores it in URL buffer
    while(fgets(urlbuffer, URLLEN, fp) != NULL)
    {

        if(seedlinksnum == 10)
        {
            printf("Max amount of seedlinks entered.\n");
            break;
        }
        if(urlbuffer[0] == '\n')
            continue;
        urlbuffer[strcspn(urlbuffer, "\n")] = 0; // remove newline character
        strcpy(urls[seedlinksnum], urlbuffer); // copy the URLS in the buffer to pointer array of URLS
        seedlinksnum++; // increment URLs array count
    }
    fclose(fp); // closing file
    pthread_t tid[seedlinksnum]; // threads
    int i;

  // initialize libcurl before starting threads
    curl_global_init(CURL_GLOBAL_ALL);

    // initalizing the threads
    for(i = 0; i< seedlinksnum; i++)
    {
        int error = pthread_create(&tid[i], NULL, crawler, (void *)urls[i]);

    if(0 != error)
      fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, error); // error message if thread could not be ran
    else
      fprintf(stderr, "Thread %d, gets %s\n", i, urls[i]); // success message
      fflush(stdout);
      sleep(1); // slow down terminal
    }

  // Waiting for all threads to terminate
    for(i = 0; i< seedlinksnum; i++)
    {
        pthread_join(tid[i], NULL);
        fprintf(stderr, "Thread %d terminated\n", i);
    }

    curl_global_cleanup(); // clean up memory and end curl
    fclose(datafile); // close file
    pthread_mutex_destroy(&lock); // destroy mutex lock
    // free URL memory
    for(int b = 0; b < seedlinksnum; b++)
    {
        free(urls[b]);
    }
    return 0; // program finished


}
