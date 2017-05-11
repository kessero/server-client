#define _GNU_SOURCE
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include "iniparser.h"
//https://github.com/ndevilla/iniparser
#define MAC_STRING_LENGTH 13
#define ERROR(fmt, ...) do { printf(fmt, __VA_ARGS__); return -1; } while(0)
void *connection_handler(void *);
const char *printData(char *);
typedef struct collectData {
    char *interface_name;
}client;
//skatalogowane interfejsy danego servera - nazwy
struct collectData interfaces[5];
int licznik = 0;
char *filename = "config";
int port, max_users;
int limit_users = 0;

//sprawdza/tworzy plik config
void create_config_file(char * name_config){
    FILE    *   ini ;

    if(!access(name_config, F_OK)){
    }else{
      //domyslny config
    ini=fopen(name_config, "w");
    fprintf(ini,
    "#\n"
    "# Config server\n"
    "#\n"
    "\n"
    "[Config]\n"
    "\n"
    "PORT       = 8888 ;\n"
    "MAX_USERS = 5 ;\n"
    "\n");
    fclose(ini);
  }
}
//parsuje plik config
int parse_config_file(char * ini_name){
    dictionary  *   ini ;

    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }
    iniparser_dump(ini, stderr);

    /* Ustawienia w config'u */
    printf("Configuration Server:\n");

    port = iniparser_getint(ini, "config:port", -1);
    printf("PORT:       [%d]\n", port);
    max_users = iniparser_getint(ini, "config:max_users", -1);
    printf("MAX_USERS: [%d]\n", max_users);

    iniparser_freedict(ini);
    return 0 ;
}
//funkcja sprawdza czy interface jest już w tabeli z interfaces
int already_there(char *interfacename) {
  for(int i=0; i<5; i++){
    if(interfaces[i].interface_name == NULL){
      return 0;
    }else if(strcmp(interfaces[i].interface_name, interfacename)== 0){
        return 1;
      }
    }
}
//funkcja sprawdza status danego interfejsu
int check_status(char *ifname) {
    int state = -1;
    int socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socId < 0) ERROR("Socket failed. Errno = %d\n", errno);

    struct ifreq if_req;
    (void) strncpy(if_req.ifr_name, ifname, sizeof(if_req.ifr_name));
    int rv = ioctl(socId, SIOCGIFFLAGS, &if_req);
    close(socId);

    if ( rv == -1) ERROR("Ioctl failed. Errno = %d\n", errno);

    return (if_req.ifr_flags & IFF_UP) && (if_req.ifr_flags & IFF_RUNNING);
}
//f.pobiera mac adres
char *getmac(char *iface){

  char *ret = malloc(MAC_STRING_LENGTH);
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

  strcpy(s.ifr_name, iface);
  if (fd >= 0 && ret && 0 == ioctl(fd, SIOCGIFHWADDR, &s))
  {
    int i;
    for (i = 0; i < 6; ++i)
      snprintf(ret+i*2,MAC_STRING_LENGTH-i*2,"%02x",(unsigned char) s.ifr_addr.sa_data[i]);
  }
  else
  {
    perror("malloc/socket/ioctl failed");
    exit(1);
  }
  return(ret);
}
//f. ustawia ip i maske adres interfejsu
void set_new_interface_data(char *inter_data){
  char nazwa[8], ip[16], mask[16];
  strcpy(nazwa, strtok(inter_data, ":"));
  strcpy(ip, strtok(NULL, ":"));
  strcpy(mask, strtok(NULL, ":"));
  struct ifreq ifr;
  struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
  const char * name = nazwa;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  int ip_check, mask_check;
  strncpy(ifr.ifr_name, name, IFNAMSIZ);
  ifr.ifr_addr.sa_family = AF_INET;
    ip_check = inet_pton(AF_INET, ip , ifr.ifr_addr.sa_data + 2);
    ioctl(fd, SIOCSIFADDR, &ifr);
    mask_check = inet_pton(AF_INET, mask , ifr.ifr_addr.sa_data + 2);
    ioctl(fd, SIOCSIFNETMASK, &ifr);

    //do rozbudowy
    if(ip_check <=0 || mask_check <= 0){
        printData("error");
      }


    ioctl(fd, SIOCGIFFLAGS, &ifr);
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    ioctl(fd, SIOCSIFFLAGS, &ifr);
    printData("ok");
}
//tworzy tablice z dostepnymi interfejsami - interfaces
char *list_interfaces(char *msg){
  char *recv_msg =(char *) malloc(sizeof(char) *(100+1));
  struct ifaddrs *ifaddr, *ifa;

  int family, s, n;
  char host[NI_MAXHOST];
  char mask[NI_MAXHOST];
  if (getifaddrs(&ifaddr) == -1) {
      perror("getifaddrs");
      exit(EXIT_FAILURE);
  }

  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
      if (ifa->ifa_addr == NULL)
          continue;

      family = ifa->ifa_addr->sa_family;
      if (family == AF_INET || family == AF_INET6 || family == AF_PACKET) {
        if(already_there(ifa->ifa_name)==0){
        interfaces[n].interface_name = ifa->ifa_name;
        licznik ++;
      }
    }
  }

  if(strcmp (msg, "list") == 0){
    for(int i=0; i<licznik; i++){
        recv_msg = strcat(strcat(recv_msg, interfaces[i].interface_name),"\n");
      }
  }
  else if(strcmp (msg, "ip") == 0){

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        for(int i=0; i<licznik; i++){
        if((strcmp(ifa->ifa_name,interfaces[i].interface_name)==0)&&(ifa->ifa_addr->sa_family==AF_INET))
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            recv_msg = strcat(strcat(strcat(strcat(recv_msg, ifa->ifa_name), "\t"),host),"\n");
        }
        }
    }
  }
  else if(strcmp (msg, "mac") == 0){
    for(int i =0; i<licznik; i++){
      char *mac = getmac(interfaces[i].interface_name);
        recv_msg = strcat(strcat(strcat(strcat(recv_msg, interfaces[i].interface_name), "\t"),mac), "\t\n");
        free(mac);
    }

  }  else if(strcmp (msg, "mask") == 0){

      for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
      {
          if (ifa->ifa_netmask == NULL)
              continue;

          s=getnameinfo(ifa->ifa_netmask,sizeof(struct sockaddr_in),mask, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
          for(int i=0; i<licznik; i++){
          if((strcmp(ifa->ifa_name,interfaces[i].interface_name)==0)&&(ifa->ifa_addr->sa_family==AF_INET))
          {
              if (s != 0)
              {
                  printf("getnameinfo() failed: %s\n", gai_strerror(s));
                  exit(EXIT_FAILURE);
              }
              recv_msg = strcat(strcat(strcat(strcat(recv_msg, ifa->ifa_name), "\t"),mask),"\n");
          }
          }
      }
    }
  else if(strcmp (msg, "stat") == 0){
    char *status_s =malloc(sizeof(char) *100);
    char up[] = "\tUP\t";
    char down[] = "\tDOWN\t";
      for(int i =0; i<licznik; i++){
          if(check_status(interfaces[i].interface_name) == 0){
            status_s = strcat(strcpy(status_s, interfaces[i].interface_name), down);
            status_s = strcat(status_s, "\n");
          }else{
            status_s = strcat(strcpy(status_s, interfaces[i].interface_name), up);
            status_s = strcat(status_s, "\n");
          }
          recv_msg = strcat(recv_msg, status_s);

      }
     free(status_s);
    }
  else if(strcmp (msg, "help") == 0){
    recv_msg = strcpy(recv_msg,"list\nip\nmask\nstat\nmac\nsetinter(set new ip and mask addres) \nquit\n");
  }
  else if(strcmp (msg, "quit") == 0){
       recv_msg = strcat(recv_msg, "quit");
  }else{
   char komu[30] = "Error - bledne polecenie!!!\n";
   recv_msg = strcpy(recv_msg, komu);
  }

  freeifaddrs(ifaddr);
  return recv_msg;
}

//wyświetlanie danych
const char * printData(char *client_m){
  char *rec_msg =(char *) malloc(sizeof(char) *(1999+1));
  char *odpow;
  if(strcmp (client_m, "help") == 0){
    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "list") == 0){

    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "ip") == 0){
    odpow = list_interfaces( client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "mask") == 0){
    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "stat") == 0){
    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "quit") == 0){
    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
  else if(strcmp (client_m, "setinter") == 0){
  //  odpow = list_interfaces(client_m);
  //  rec_msg = strcpy(rec_msg, odpow);
  //  free(odpow);
  }else if(strcmp (client_m, "mac") == 0){
    odpow = list_interfaces(client_m);
    rec_msg = strcpy(rec_msg, odpow);
    free(odpow);
  }
   else{
     odpow = list_interfaces(client_m);
     rec_msg = strcpy(rec_msg, odpow);
     free(odpow);
  }
  return rec_msg;

}
//funkcja zarzadza polaczeniami
void *connection_handler(void *socket_desc){

	int sock = *(int*)socket_desc;
	int read_size;
	char *message , client_message[2000];
  char *odp;

  //wiadomość dla klienta
	message = "Witaj!\n";
	write(sock, message, strlen(message));

	//Odpowiedź klienta
	while( ( read_size = recv(sock , client_message , 2000 ,0))>0 )	{
    for (int i=0; client_message[i]; i++){
     client_message[i] = tolower(client_message[i]);
    }
    if(strcmp(client_message, "setinter")== 0){
      message = "Podaj nazwe interface, IP i Maske w formacie nazwa:xxx.xxx.xxx.xxx:xxx.xxx.xxx.xxx";
      write(sock, message, strlen(message)+1);
       while ((read_size = recv(sock , client_message , 2000 ,0))>0){
         set_new_interface_data(client_message);
       }
    }else{
    odp = printData(client_message);
      write(sock, odp, strlen(odp)+1);
      memset(client_message, 0, sizeof(client_message));
      free(odp);
      fflush(stdout);
        }
      }

	if(read_size == 0)
	{
		puts("Klient rozłączył się");
    limit_users --;
		fflush(stdout);
	}
	else if(read_size == -1)
	{
		perror("odpowiedź błędna");
	}
	//zwolnić gniazdo
  free(socket_desc);

	return 0;
}

int main(int argc, char *argv[]){

  create_config_file(filename);
  parse_config_file(filename);
	int socket_desc, client_sock, c , *new_sock;
	struct sockaddr_in server , client;

	//tworzymy gniazdo
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1)
	{
		printf("Nie mogę utworzyć gniazda");
	}
	puts("Gniazdo utworzone");

	//Struktura socket_in
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	//bind
	if( bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) <0)
	{
		perror("Bind fail");
		return 1;
	}
	puts("Bind OK");

	//nasłuch
	listen(socket_desc , 1);
	puts("Czekam na połączenie...");
	c = sizeof(struct sockaddr_in);

	//akceptacja lub odrzucenie połączenia

	while((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
	{
    if(limit_users > max_users){
      perror("Limit");
    }else{
		puts("Połączenie zaakceptowane");
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;


		if(pthread_create( &sniffer_thread , NULL , connection_handler , (void*) new_sock) < 0)
		{
			perror("Nie udało się utworzyć wątku");
			return 1;
		}
		puts("Utworzono wątek");
    limit_users++;
    }
	if (client_sock < 0)
	{
		perror("Błąd połączenia");
		return 1;
	}
}
	return 0;
}
