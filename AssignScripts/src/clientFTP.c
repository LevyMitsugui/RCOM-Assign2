/**
 * Example code for getting the IP address from hostname.
 * tidy up includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_FIELD 256

typedef struct {
    char user[MAX_FIELD];
    char password[MAX_FIELD];
    char host[MAX_FIELD];
    char path[MAX_FIELD];
} ftp_url_info;

#include <stdio.h>
#include <string.h>

#define DEBUG
#define BUFF_SIZE 1024

int parse_url(const char *url, ftp_url_info *info);
int connect_control_socket(const char *hostname);
int send_command(int sockfd, const char *cmd);
int read_response(int sockfd, char *buffer, size_t size);
int login(int sockfd, const char *user, const char *password);
int enter_passive_mode(int ctrl_sock, char *pasv_ip, int *pasv_port);
int connect_data_socket(const char *ip, int port);
int retrieve_file(int ctrl_sock, int data_sock, const char *remote_path, const char *local_filename);
const char *get_filename_from_path(const char *path);
long get_file_size(int ctrl_sock, const char *remote_path);
void print_progress_bar(long current, long total);

int main(int argc, char *argv[]) {
    struct hostent *h;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FTP url standard RFC1738: 'ftp://[<user>:<password>@]<host>/<url-path>'>\n", argv[0]);
        exit(-1);
    }


    const char *url = argv[1];
    ftp_url_info info; // to store ftp user info

    if (parse_url(url, &info) != 0) { //Parse arg. If no User or Password, it will be anonymous. Save in ftp_url_info
        exit(-1);
    }

    #ifdef DEBUG
        printf("Host name  : %s\n", info.host);
        printf("Username   : %s\n", info.user);
        printf("Password   : %s\n", info.password);
        printf("Path       : %s\n", info.path);
    #endif
    
    int sockfd = connect_control_socket(info.host); // uses hostname to connect to control socket
    
    if(sockfd < 0) {
        perror("connect_control_socket");
        exit(-1);
    }

    char buffer[BUFF_SIZE];
    // recv(sockfd, buffer, sizeof(buffer), 0); // gets the server response
    // buffer[sizeof(buffer) - 1] = '\0';
    
    // #ifdef DEBUG
    //     printf("%s\n", buffer);
    // #endif

    int code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 220) {
        fprintf(stderr, "Unexpected response: %s", buffer);
        return -1;
    }
    
    printf("220 - Connected to %s\n", info.host);

    if(login(sockfd, info.user, info.password)<0) exit(1);
    
    char data_ip[64];
    int data_port;
    if(enter_passive_mode(sockfd, data_ip, &data_port)==0){


        int data_sock = connect_data_socket(data_ip, data_port);
        if (data_sock >= 0) {
            const char *filename = get_filename_from_path(info.path);
            retrieve_file(sockfd, data_sock, info.path, filename);
        }
    }
    
    /**
    * The struct hostent (host entry) with its terms documented

    struct hostent {
        char *h_name;    // Official name of the host.
        char **h_aliases;    // A NULL-terminated array of alternate names for the host.
        int h_addrtype;    // The type of address being returned; usually AF_INET.
        int h_length;    // The length of the address in bytes.
        char **h_addr_list;    // A zero-terminated array of network addresses for the host.
        // Host addresses are in Network Byte Order.
    };

    #define h_addr h_addr_list[0]	The first address in h_addr_list.
    */

    return 0;
}

int parse_url(const char *url, ftp_url_info *info) {
    const char *prefix = "ftp://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) {
        fprintf(stderr, "Invalid URL: must start with ftp://\n");
        return -1;
    }

    const char *p = url + strlen(prefix);
    const char *at = strchr(p, '@');
    const char *slash;

    if (at) {
        // With user and password
        const char *colon = strchr(p, ':');
        if (!colon || colon > at) {
            fprintf(stderr, "Invalid URL: missing password\n");
            return -1;
        }

        strncpy(info->user, p, colon - p);
        info->user[colon - p] = '\0';

        strncpy(info->password, colon + 1, at - colon - 1);
        info->password[at - colon - 1] = '\0';

        p = at + 1; // Now p points to the host
    } else {
        // Anonymous login
        strcpy(info->user, "anonymous");
        strcpy(info->password, "anonymous");
    }

    slash = strchr(p, '/');
    if (!slash) {
        fprintf(stderr, "Invalid URL: missing path\n");
        return -1;
    }

    strncpy(info->host, p, slash - p);
    info->host[slash - p] = '\0';

    strcpy(info->path, slash + 1); // Skip the slash

    return 0;
}

int connect_control_socket(const char *hostname) {
    struct hostent *he;
    struct sockaddr_in server_addr;
    int sockfd;

    if ((he = gethostbyname(hostname)) == NULL) {
        perror("gethostbyname");
        return -1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21);  // FTP control port
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int send_command(int sockfd, const char *cmd) {
    return send(sockfd, cmd, strlen(cmd), 0);
}

int read_response(int sockfd, char *buffer, size_t size) {
    int bytes = recv(sockfd, buffer, size - 1, 0);
    if (bytes < 0) return -1;
    buffer[bytes] = '\0';

    char code_str[4] = {0}; // To hold the 3-digit code and null terminator
    if (bytes < 3) {
        fprintf(stderr, "Received too short response: %d bytes\n", bytes);
        return -1;
    }

    strncpy(code_str, buffer, 3);
    code_str[4] = '\0';

    int code = atoi(code_str);
    printf("Code from server: %d\n", code);
    return code;  // Parse 3-digit code at start
}


int login(int sockfd, const char *user, const char *password) {
    char buffer[1024];

    // Send USER
    char user_cmd[512];
    snprintf(user_cmd, sizeof(user_cmd), "USER %s\r\n", user);
    send_command(sockfd, user_cmd);

    int code = read_response(sockfd, buffer, sizeof(buffer));
    if (code != 331 && code != 230) {
        fprintf(stderr, "USER failed: %s", buffer);
        return -1;
    }

    // Send PASS if needed
    if (code == 331) {
        printf("331 - User name okay, need password.\n");

        char pass_cmd[512];
        snprintf(pass_cmd, sizeof(pass_cmd), "PASS %s\r\n", password);
        send_command(sockfd, pass_cmd);

        code = read_response(sockfd, buffer, sizeof(buffer));
        if (code != 230) {
            fprintf(stderr, "PASS failed: %s", buffer);
            return -1;
        }
        printf("230 - Login successful.\n");
    }

    #ifdef DEBUG
        printf("Login successful.\n");
    #endif
    return 0;
}

int enter_passive_mode(int ctrl_sock, char *pasv_ip, int *pasv_port) {
    char buffer[1024];
    send(ctrl_sock, "PASV\r\n", strlen("PASV\r\n"), 0);

    int code = read_response(ctrl_sock, buffer, sizeof(buffer));
    if (code != 227) {
        fprintf(stderr, "PASV failed: %s", buffer);
        return -1;
    }

    printf("227 - Entering Passive Mode.\n");

    // Parse the PASV response
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
               &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        fprintf(stderr, "Failed to parse PASV response: %s", buffer);
        return -1;
    }

    printf("Host: %d.%d.%d.%d\n", h1, h2, h3, h4);
    printf("Port: %d\n", p1 * 256 + p2);

    sprintf(pasv_ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *pasv_port = p1 * 256 + p2;

    return 0;
}

int connect_data_socket(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int retrieve_file(int ctrl_sock, int data_sock, const char *remote_path, const char *local_filename) {
    char buffer[1024];
    char retr_cmd[512];

    long total_size = get_file_size(ctrl_sock, remote_path);
    if (total_size < 0){
        printf("Warning: Could not determine file size.\n");
    }
    // Send RETR command
    snprintf(retr_cmd, sizeof(retr_cmd), "RETR %s\r\n", remote_path);
    send(ctrl_sock, retr_cmd, strlen(retr_cmd), 0);

    // Check if RETR is accepted
    int code = read_response(ctrl_sock, buffer, sizeof(buffer));
    if (code != 150 && code != 125) {
        fprintf(stderr, "RETR failed: %s", buffer);
        return -1;
    }

    // Open local file for writing
    FILE *fp = fopen(local_filename, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Read from data socket and write to file
    int bytes;
    long downloaded = 0;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
        downloaded += bytes;
        if (total_size > 0) print_progress_bar(downloaded, total_size);
    }
    printf("\n");

    fclose(fp);
    close(data_sock);

    // Wait for final 226 Transfer complete
    code = read_response(ctrl_sock, buffer, sizeof(buffer));
    if (code != 226) {
        fprintf(stderr, "Transfer not completed cleanly: %s", buffer);
        return -1;
    }

    #ifdef DEBUG
        printf("Download successful.\n");
    #endif
        return 0;
}

const char *get_filename_from_path(const char *path) {
    const char *slash = strrchr(path, '/');
    return (slash) ? slash + 1 : path;
}

long get_file_size(int ctrl_sock, const char *remote_path) {
    char buffer[1024];
    char size_cmd[512];

    snprintf(size_cmd, sizeof(size_cmd), "SIZE %s\r\n", remote_path);
    send(ctrl_sock, size_cmd, strlen(size_cmd), 0);

    int code = read_response(ctrl_sock, buffer, sizeof(buffer));
    if (code != 213) return -1;

    long size = 0;
    sscanf(buffer, "213 %ld", &size);
    return size;
}

void print_progress_bar(long current, long total) {
    const int bar_width = 50;
    float progress = (float)current / total;
    int pos = (int)(bar_width * progress);

    printf("\r[");
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %ld/%ld bytes (%.1f%%)", current, total, progress * 100);
    fflush(stdout);
}