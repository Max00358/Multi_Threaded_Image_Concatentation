#include <curl/curl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "catpng.h"

void * png_thread(void * arg);
RECV_BUF * get_png_snippet(void);

RECV_BUF * recv_data_arr[50];
atomic_uint png_count = ATOMIC_VAR_INIT(0);
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

char IMG_URL[] = "http://ece252-1.uwaterloo.ca:2520/image?img=1";

int main(int argc, char ** argv) {
  int c;
  int t = 1;
  int n = 1;
  char * str = "option requires an argument";

  while ((c = getopt(argc, argv, "t:n:")) != -1) {
    switch (c) {
    case 't':
      t = strtoul(optarg, NULL, 10);
      printf("option -t specifies a value of %d.\n", t);
      if (t <= 0) {
        fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
        return -1;
      }
      break;
    case 'n':
      n = strtoul(optarg, NULL, 10);
      printf("option -n specifies a value of %d.\n", n);
      if (n <= 0 || n > 3) {
        fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
        return -1;
      }
      break;
    default:
      continue;
    }
  }

  for (int i = 0; i < 50; i++)
    recv_data_arr[i] = NULL;

  unsigned server_id = (unsigned) n;
  unsigned thread_count = (unsigned) t;
  if (thread_count > 500) {
    printf("Thread count exceeded\n");
    return -1;
  }
  pthread_t thread_id_arr[thread_count];

  IMG_URL[14] = (char) server_id + '0';

  curl_global_init(CURL_GLOBAL_DEFAULT);

  unsigned failed_threads = 0;
  for (unsigned i = 0; i < thread_count; i++) {
    int res = pthread_create(thread_id_arr + i, NULL, png_thread, NULL);
    if (res) {
      --i;
      ++failed_threads;
    }
    if (failed_threads > 100) {
      thread_count = i;
      break;
    }
  }

  for (unsigned i = 0; i < thread_count; i++)
    pthread_join(thread_id_arr[i], NULL);

  int result = catpng(recv_data_arr, 50);
  if (!result) {
    printf("Success\n");
  } else {
    printf("Failure\n");
  }

  /* Clean Up */
  for (unsigned i = 0; i < 50; i++) {
    if (recv_data_arr[i]) {
      recv_buf_cleanup(recv_data_arr[i]);
      free(recv_data_arr[i]);
    }
  }
  curl_global_cleanup();

  return 0;
}

void * png_thread(void * arg) {
  unsigned err_count = 0;
  while (atomic_load( & png_count) < 50) {
    /*if (err_count > 500000) {
    	printf("Err Count Exceeded\n");
    	return NULL;
    }*/

    RECV_BUF * p_recv_data = get_png_snippet();
    if (!p_recv_data) {
      ++err_count;
      continue;
    }
    if (p_recv_data -> seq < 0 || p_recv_data -> seq > 49) {
      ++err_count;
      free(p_recv_data);
      continue;
    }

    pthread_mutex_lock( & thread_mutex);
    if (!recv_data_arr[p_recv_data -> seq]) {
      recv_data_arr[p_recv_data -> seq] = p_recv_data;
      atomic_fetch_add( & png_count, 1);
    } else {
      recv_buf_cleanup(p_recv_data);
      free(p_recv_data);
    }
    pthread_mutex_unlock( & thread_mutex);
  }

  return NULL;
}

RECV_BUF * get_png_snippet(void) {
  CURL * curl_handle;
  CURLcode res;

  RECV_BUF * p_recv_buf = malloc(sizeof(RECV_BUF));
  recv_buf_init(p_recv_buf, BUF_SIZE);

  curl_handle = curl_easy_init();

  if (curl_handle == NULL) {
    recv_buf_cleanup(p_recv_buf);
    free(p_recv_buf);
    return NULL;
  }

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, IMG_URL);
  /* register write call back function to process received data */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
  /* user defined data structure passed to the call back function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void * ) p_recv_buf);
  /* register header call back function to process received header data */
  curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
  /* user defined data structure passed to the call back function */
  curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void * ) p_recv_buf);
  /* some servers requires a user-agent field */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  /* get it! */
  res = curl_easy_perform(curl_handle);

  /* cleaning up */
  curl_easy_cleanup(curl_handle);
  /* recv_buf_cleanup(&recv_buf); */

  if (res != CURLE_OK) {
    recv_buf_cleanup(p_recv_buf);
    free(p_recv_buf);
    return NULL;
  }

  return p_recv_buf;
}
