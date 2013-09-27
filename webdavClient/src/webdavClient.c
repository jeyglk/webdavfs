//
//     WebdavClient, the client deamon of Webdavfs, a WebDAV virtual filesystem
//     Proof of concept
//     Copyright (C) 2013  Jeremy Mathevet
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <signal.h>
#include <linux/netlink.h>
#include "xml.h"

#define MAXLINE 4096
#define MAXSUB 2000
#define NETLINK_NITRO 17
#define MAX_PAYLOAD 2048


int sockfd = 0, n = 0;
uint8_t recvBuff[256];
struct sockaddr_in serv_addr; 

char sendline[MAXLINE + 1], recvline[MAXLINE + 1];
char* page = "/owncloud/remote.php/webdav/";
char* host = "127.0.0.1:80";
// user: test and password: test concatenated and base 64 encoded
char* auth = "dGVzdDp0ZXN0";
uint8_t response[10000];

int fd;

struct sockaddr_nl s_nladdr, d_nladdr;
struct msghdr msg ;
struct nlmsghdr *nlh=NULL ;
struct iovec iov;

uint8_t* remote_message;
uint8_t* remote_file_name;

struct resource_parser {
	uint8_t* buffer;
	size_t position;
	size_t length;
};

struct remote_request {
	char request;
	char* filename;
};

unsigned int resources_number;




char* substr(char* buf, const char* s, size_t beg, size_t len)
{
	  memcpy(buf, s + beg, len);
	  buf[len] = '\0';
	  return (buf);
}




/* Generate a PROPFIND request
 */
static void request_propfind() {

  snprintf(sendline, MAXSUB, 
    "PROPFIND %s HTTP/1.1\n"
    "Host: %s\n"
    "Authorization: Basic %s\n"
    "\n", page, host, auth);
}


/* Generate a MKCOL request
 */
static void request_mkcol(char* directory) {
  
  printf("request_mkcol: directory %s\n\n", directory);
  snprintf(sendline, MAXSUB, 
    "MKCOL %s%s HTTP/1.1\n"
    "Host: %s\n"
    "Authorization: Basic %s\n"
    "\n", page, directory, host, auth);
}


/* Generate a DELETE request
 */
static void request_delete(uint8_t* filename) {

  snprintf(sendline, MAXSUB, 
    "DELETE %s%s HTTP/1.1\n"
    "Host: %s\n"
    "Authorization: Basic %s\n"
    "\n", page, filename, host, auth);
}


/* Generate a PUT request
 * Like a touch on Linux: a PUT on a nonexistent file create the file (empty).
 */
static void request_put(char* filename) {

  snprintf(sendline, MAXSUB, 
    "PUT %s%s HTTP/1.1\n"
    "Host: %s\n"
    "Authorization: Basic %s\n"
    "\n", page, filename, host, auth);
}


/* Generate a LOCK request
 * Like a touch on Linux: a LOCK on a nonexistent file create the file (empty).
 */
static void request_lock(uint8_t* filename) {

  snprintf(sendline, MAXSUB, 
    "LOCK %s%s HTTP/1.1\n"
    "Host: %s\n"
    "Authorization: Basic %s\n"
    "Content-Type: text/xml; charset=utf-8\n"
    "Content-Length: 167\n"
    "\n"
    "<?xml version=\"1.0\"?>\n"
    "<d:lockinfo xmlns:d=\"DAV:\">\n"
	"<d:locktype><d:write/></d:locktype>\n"
	 "<d:lockscope><d:exclusive/></d:lockscope>\n"
	 "<d:owner>root</d:owner>\n"
      "</d:lockinfo>\n"
      "\n", page, filename, host, auth);
  
  printf("%s",sendline);
}





struct remote_request parse_remote_request(uint8_t* message) {

	struct resource_parser parser = {
		.buffer = message,
		.position = 1,
		.length = strlen(message)
	};
	
	struct remote_request r = {
		.request = NULL,
		.filename = NULL
	};
	
	size_t start = parser.position;
	size_t length = 0;
	
	// Fill request
	while (parser.position < parser.length) {
		if ('?' != parser.buffer[parser.position]) {
			parser.position++;
		} else {
			r.request = parser.buffer[parser.position - 1];
			printf("request: %c\n", r.request);
			parser.position++;
			start = parser.position;
			break;
		}
	}
	
	// Fill filename
	while (parser.position < parser.length) {
		if ('?' != parser.buffer[parser.position]) {
			parser.position++;
		} else {
			char* buf = alloca((parser.position - start + 1) * sizeof(char));
			//r.filename = substr(buf, parser.buffer, start, parser.position - start);
			r.filename = strndup(parser.buffer + start, parser.position - start);
			printf("filename: %s\n", r.filename);
			break;
		}
	}
	
	return r;
}





static void sendwebdavRequest() {

  memset(recvBuff, '0',sizeof(recvBuff));
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
      printf("\n Error : Could not create socket \n");
      //return 1;
  } 

  memset(&serv_addr, '0', sizeof(serv_addr)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(80); 

 // if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
  if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
  {
      printf("\n inet_pton error occured\n");
      //return 1;
  } 

  if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
      printf("\n Error : Connect Failed \n");
      //return 1;
  }
  
  // Write the request
  printf("%s", sendline);
  if (write(sockfd, sendline, strlen(sendline))>= 0) 
  {
    
	FILE *input= fdopen(sockfd, "r");
		if(input== NULL)
		{
		printf("Error");
		//	return 1;
		}

	while(fgets(response, sizeof(response), input) != NULL)
	{
		printf("%s", response);
	}
  }
}





void sendNetlinkMessage(uint8_t* message) {
  
 
  struct sockaddr_nl d_nladdr; //s_nladdr;
  struct msghdr msg ;
  struct nlmsghdr *nlh=NULL ;
  struct iovec iov;
  int fd_local = socket(AF_NETLINK , SOCK_RAW , NETLINK_NITRO );
  

  /* destination address */
  memset(&d_nladdr, 0 ,sizeof(d_nladdr));
  d_nladdr.nl_family = AF_NETLINK ;
  d_nladdr.nl_pad= 0;
  d_nladdr.nl_pid = 0; /* destined to kernel */
  
  /* Fill the netlink message header */
  nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
  memset(nlh , 0 , NLMSG_SPACE(MAX_PAYLOAD));
  strcpy(NLMSG_DATA(nlh), message);
  nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
  nlh->nlmsg_pid = getpid();
  nlh->nlmsg_flags = 1;
  nlh->nlmsg_type = 0;
  
  /*iov structure */
  iov.iov_base = (void *)nlh;
  iov.iov_len = nlh->nlmsg_len;

  /* msg */
  memset(&msg,0,sizeof(msg));
  msg.msg_name = (void *) &d_nladdr ;
  msg.msg_namelen= sizeof(d_nladdr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  sendmsg(fd_local, &msg, 0);
  
  close(fd_local);
}





void handleNetlink(void * message) {
  
	struct remote_request r;
   
	// Remote request
	if( '?' == *(char *) message) {
		printf("kernel request detected\n");
		r = parse_remote_request(message);
	
		if ('m' == r.request) {
			printf("mkcol %s\n\n", r.filename);
			request_mkcol(r.filename);
			sendwebdavRequest();
		}
		else if ('f' == r.request) {
			printf("put new file %s\n\n", r.filename);
			request_put(r.filename);
			sendwebdavRequest();
		}
	}
}




void waitNetlink() {


	int fd = socket(AF_NETLINK,SOCK_RAW,NETLINK_NITRO);
	
	/* source address */
	memset(&s_nladdr, 0 ,sizeof(s_nladdr));
	s_nladdr.nl_family = AF_NETLINK ;
	s_nladdr.nl_pad = 0;
	s_nladdr.nl_pid = getpid();
	bind(fd, (struct sockaddr*)&s_nladdr, sizeof(s_nladdr));

	/* Read message from kernel */
	printf("Wating for message from kernel\n");
	//memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	recvmsg(fd, &msg, 0);
	printf("\n");
	printf("Received data from Kernel: [%s]", NLMSG_DATA(nlh));
	printf("\n");
      
	// Remote request
	handleNetlink(NLMSG_DATA(nlh));
	    
	close(fd);
	
	waitNetlink();

}



s
void initNetlink() {


	int fd = socket(AF_NETLINK,SOCK_RAW,NETLINK_NITRO);
	
	/* source address */
	memset(&s_nladdr, 0 ,sizeof(s_nladdr));
	s_nladdr.nl_family = AF_NETLINK ;
	s_nladdr.nl_pad = 0;
	s_nladdr.nl_pid = getpid();
	bind(fd, (struct sockaddr*)&s_nladdr, sizeof(s_nladdr));
	
	/* destination address */
	memset(&d_nladdr, 0 ,sizeof(d_nladdr));
	d_nladdr.nl_family = AF_NETLINK ;
	d_nladdr.nl_pad= 0;
	d_nladdr.nl_pid = 0; /* destined to kernel */
	
	/* Fill the netlink message header */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh , 0 , NLMSG_SPACE(MAX_PAYLOAD));
	strcpy(NLMSG_DATA(nlh), "?");
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 1;
	nlh->nlmsg_type = 0;
	
	/*iov structure */
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;

	/* msg */
	memset(&msg,0,sizeof(msg));
	msg.msg_name = (void *) &d_nladdr ;
	msg.msg_namelen= sizeof(d_nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	sendmsg(fd, &msg, 0);
	
	
	/* Read message from kernel */
	
	printf("Wating for message from kernel\n");
	//memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	recvmsg(fd, &msg, 0);
	printf("\n");
	printf("Received data from Kernel: [%s]", NLMSG_DATA(nlh));
	printf("\n");
	
	handleNetlink(NLMSG_DATA(nlh));
	      
	close(fd);
	
	waitNetlink();

}






static void create_resources(struct xml_node* root) {

	uint8_t* buffer;
	
	uint8_t* href_content;
	char* filename;
	char type;
	uint8_t* length_content;
	
	int i;
	for (i = 1; i < resources_number; i++) {
		/** response tag
		 */
		struct xml_node* response_node = xml_node_child(root,i);
		 
		/** easy href
		 */
		href_content = xml_easy_content(xml_easy_child(response_node, "d:href", 0));
		printf("Location: %s\n", href_content);
		 
		filename = href_content + strlen("/owncloud/remote.php/webdav/");
		printf("File Name: %s\n", filename);
		 
		/** easy resource type
		 */
		struct xml_node* resourceType = xml_easy_child(response_node,"d:propstat", "d:prop",
								"d:resourcetype", 0);
		if (resourceType) {
			type = 'c';
			filename[strlen(filename)-1] = 0;
			printf("Type: collection\n");
		} else {
			type = 'f';
			printf("Type: file\n");
			
			/** easy file size
			 */
			length_content = xml_easy_content(xml_easy_child(response_node,
										  "d:propstat",
							     "d:prop", "d:getcontentlength", 0));
			printf("File size: %s bits\n", length_content);
		  
		}
		
		uint8_t* message = malloc(snprintf(NULL, 0,"%s?%s?%c?%s?",
						  href_content, filename, type, length_content) +1);
		sprintf(message,"%s?%s?%c?%s?", href_content, filename, type, length_content);
		printf("%s\n",message);
		
		sendNetlinkMessage(message);
		
// 		free(href_content);
// 		free(length_content);
		 
		printf("\n");
	}
	free(buffer);
}



/** 
 * Parse the answer sent by the server
 */
static void parse_document(uint8_t* response) {
  
	struct xml_document* document = xml_parse_document(response, strlen(response));
	
	if (!document) {
		printf("Could not parse document\n");
		exit(EXIT_FAILURE);
	}

	struct xml_node* root = xml_document_root(document);
	resources_number = (unsigned int)xml_node_children(root);
	
	/* Count how many resources has the current collection, as a <response> equals a resource
	* We substract one, as we also get a <response> for the current location.
	*/
	printf("This collection has %i elements (files or folders)\n\n", resources_number - 1);
	
	create_resources(root);
	
	xml_document_free(document, false);
	
}



void intHandler() {
    close(fd);
    exit(0);
}



int main(int argc, char *argv[])
{

  signal(SIGINT, intHandler);
  
  request_propfind();
  sendwebdavRequest();
  parse_document(response);
  initNetlink();
  
  
  return 0;
}
