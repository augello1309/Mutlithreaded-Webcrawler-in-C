To compile this code, use this command: gcc crawler_finished.c -g $(pkg-config --cflags --libs libxml-2.0 libcurl)
Libraries Needed: libxml-2.0 and libcurl
Libxml-2.0: sudo apt-get install libxml2-dev
Libcurl: apt-get install libcurl4-openssl-dev
Note: pkg needs to be downloaded from command line
pkg: sudo apt-get install -y pkg-config
Note: Max links that can be used in file is 10.
Note: Only use one link per line
Note: Prints all failed connections to terminal. Prints all successes to file called datafile.txt
