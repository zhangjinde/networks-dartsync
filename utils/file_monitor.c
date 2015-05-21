#include "file_monitor.h"
#include "network.h"
#include "tracker_peer_table.h"


char root_directory[128];
file_node* file_table;
time_t last_table_update_time = 0;
int update_enable;

void block_update() {
    update_enable =0;
}

void unblock_update() {
    update_enable = 1;
}

void watchDirectory(char* directory) {
    sprintf(root_directory, "%s", directory);
}

void file_table_initial() {
    file_table = (file_node*)malloc(sizeof(file_node));
    bzero(file_table, sizeof(file_node));
    file_table->peers[0] = get_My_IP();
}
file_node* get_my_file_table() {
    return file_table;
}
void file_table_free(file_node* file_node_head) {
    file_node* curr = file_node_head;
    while (curr != NULL) {
        file_node* temp = curr;
        curr = curr->next;
        free(temp);
    }
}
int file_table_update() {
    if (update_enable) {
        file_table_free(file_table);
        file_table_initial();
        file_node* runner = file_table;
        
        //modified
        time_t tmpt_time;
        time(&tmpt_time);
        
        int is_updated = file_table_update_helper(root_directory, &runner);
        //time(&last_table_update_time);
        last_table_update_time = tmpt_time;
        return is_updated;
    } else {
        return 0;
    }
}
void file_table_print() {
    printf("==================Local File Table======================\n");
    file_node* runner = file_table;
    while (runner->next != NULL) {
        printf("%s\t", runner->next->name);
        int len = strlen(runner->next->name);
        if (len < 8)
            printf("\t");
        printf("%ld\t", runner->next->timestamp);
        int i;
        for (i = 0; i < runner->next->num_peers; i++) {
            printf("%s\t", inet_ntoa(*(struct in_addr*)&runner->next->peers[i]));
        }
        printf("\n");
        runner = runner->next;
    }
    printf("========================================================\n");
}
int file_table_update_helper(char* directory, file_node** last) {
    int is_updated = 0;
    struct stat attrib;
    stat(directory, &attrib);
    if (last_table_update_time <= attrib.st_mtime) {
        is_updated = 1;
    }
    DIR * dir_ptr;
    struct dirent * direntp;
    if((dir_ptr = opendir(directory))==NULL) {
        printf("Cannot open %s\n",directory);
    } else {
        while ((direntp = readdir(dir_ptr))!=NULL) {
            if (direntp->d_name[0] != '.' && direntp->d_name[strlen(direntp->d_name)-1] != '~') {//Hidden file not display
                if (direntp->d_type == FILE_TYPE) {
                    file_node* new_node = (file_node*)malloc(sizeof(file_node));
                    bzero(new_node, sizeof(file_node)); /* temp test */
                    (*last)->next = new_node;
                    (*last) = new_node;
                    sprintf(new_node->name, "%s/%s", directory, direntp->d_name);
                    // printf("%s\n", new_node->name);
                    stat(new_node->name, &attrib);
                    new_node->timestamp = attrib.st_mtime;
                    new_node->size = attrib.st_size;
                    new_node->num_peers = 1;
                    new_node->peers[0] = file_table->peers[0];
                    new_node->next = NULL;
                    if (last_table_update_time <= attrib.st_mtime) {
                        is_updated = 1;
                    }
                } else if (direntp->d_type == FOLDER_TYPE) {
                    char sub_directory[128];
                    sprintf(sub_directory, "%s/%s", directory, direntp->d_name);
                    is_updated = file_table_update_helper(sub_directory, last) || is_updated;
                } else {
                    printf("file_table_initial_helper : Unknow file type\n");
                }
            }
        }
        closedir(dir_ptr);
    }
    return is_updated;
}

void send_file_table(int socket) {
    int num_nodes = 1;
    file_node* runner = file_table;
    while (runner->next != NULL) {
        runner = runner->next;
        num_nodes++;
    }
    if (send(socket, &num_nodes, sizeof(int), 0) < 0) {
        printf("Error in send_file_table\n");
    }
    runner = file_table;
    while (runner != NULL) {
        if (send(socket, runner, sizeof(file_node), 0) < 0) {
            printf("Error in send_file_table\n");
        }
        runner = runner->next;
    }
}

void recv_file_table(int socket, file_node** new_table) {
    int num_nodes;
    if (recv(socket, &num_nodes, sizeof(int), 0) < 0 ) {
        printf("Error in recv_file_table()\n");
        return;
    }
    // printf("Get %d file nodes\n", num_nodes);
    file_node* runner = *new_table;
    
    char *buffer = (char*)malloc(sizeof(file_node));
    int len, buflen;
    printf("===============Receive remote table===================\n");
    while (num_nodes > 0) {
        buflen = 0;
        bzero(buffer, sizeof(file_node));
        if (runner == NULL) {
            runner = (file_node*)malloc(sizeof(file_node));
            while (buflen < sizeof(file_node)) {
                if ((len = recv(socket, buffer + buflen, sizeof(file_node) - buflen, 0)) < 0) {
                    printf("Error in recv_file_table\n");
                    *new_table = NULL;
                    return;
                } else {
                    buflen = buflen + len;
                    // printf("receive len %d, total length %d\n", len, buflen);
                }
            }
            memcpy(runner, buffer, buflen);
            runner->next = NULL;
            *new_table = runner;
        } else {
            file_node* new_node = (file_node*)malloc(sizeof(file_node));
            while (buflen < sizeof(file_node)) {
                if ((len = recv(socket, buffer + buflen, sizeof(file_node) - buflen, 0)) < 0) {
                    printf("Error in recv_file_table\n");
                    *new_table = NULL;
                    return;
                } else {
                    buflen = buflen + len;
                    // printf("receive len %d, total length %d\n", len, buflen);
                }
            }
            memcpy(new_node, buffer, buflen);
            new_node->next = NULL;
            runner->next = new_node;
            runner = new_node;
        }
        /* Print the file node entry */
        printf("%s\t", runner->name);
        int len = strlen(runner->name);
        if (len < 8)
            printf("\t");
        printf("%ld\t", runner->timestamp);
        int i;
        for (i = 0; i < runner->num_peers; i++) {
            printf("%s\t", inet_ntoa(*(struct in_addr*)&runner->peers[i]));
        }
        printf("\n");
        /* ---------------------------- */
        num_nodes--;
    }
    printf("===============From %s====================\n", inet_ntoa(*(struct in_addr*)&(*new_table)->peers[0]));
    free(buffer);
}

void sync_with_server(file_node* server_table) {
    update_enable = 0;
    file_node* runner_client = file_table;
    int is_updated = 0;
    while (runner_client->next != NULL) {
        file_node* runner_server = server_table;
        int is_exist = 0;
        while (runner_server->next != NULL) {
            if (strcmp(runner_client->next->name, runner_server->next->name) == 0) {
                is_exist = 1;
                break;
            }
            runner_server = runner_server->next;
        }
        if (is_exist == 0) {
            printf("%s should be deleted\n", runner_client->next->name);
            is_updated = 1;
            /* Delete the local file */
            char command[200];
            sprintf(command, "rm %s", runner_client->next->name);
            system(command);
        }
        runner_client = runner_client->next;
    }
    
    file_node* runner_server = server_table;
    while (runner_server->next != NULL) {
        runner_client = file_table;
        int is_exist = 0;
        while (runner_client->next != NULL) {
            if (strcmp(runner_client->next->name, runner_server->next->name) == 0) {
                is_exist = 1;
                break;
            }
            runner_client = runner_client->next;
        }
        if (is_exist == 0) {
            printf("%s should be added\n", runner_server->next->name);
            is_updated = 1;
            /* Download the file from peer */
            download_file_multi_thread(runner_server->next);
        } else {
            if (runner_client->next->timestamp < runner_server->next->timestamp) {
                printf("%s should be updated\n", runner_server->next->name);
                is_updated = 1;
                /* Delete the local file */
                char command[200];
                sprintf(command, "rm %s", runner_client->next->name);
                system(command);
                /* Download the file from peer */
                download_file_multi_thread(runner_server->next);
            }
        }
        runner_server = runner_server->next;
    }
    /* === If updated, refresh the local file table === */
    if (is_updated) {
        file_table_free(file_table);
        file_table_initial();
        file_node* runner = file_table;
        is_updated = file_table_update_helper(root_directory, &runner);
        last_table_update_time = 0;
    }
    /* ==== Make sure the file_table_update() function will detect the update  ==*/
    update_enable = 1;
}

/* Get new file infomation from client file table, and update the local server table. Called by the tracker. */
void sync_from_client(file_node* client_table) {
    // printf(v"file table at present::::::::::::::::\n");
    // file_table_print();
    unsigned long client_IP = client_table->peers[0];
    file_node* runner_server = file_table;
    /* Looking for file in server_table, but not in client_table */
    while (runner_server != NULL && runner_server->next != NULL) {
        file_node* runner_client = client_table;
        int is_exist = 0;
        while (runner_client->next != NULL) {
            if (strcmp(runner_server->next->name, runner_client->next->name) == 0) {
                is_exist = 1;
                break;
            }
            runner_client = runner_client->next;
        }
        if (is_exist == 0) {
            int i;
            int peer_exist = 0;
            for (i = 0; i < runner_server->next->num_peers; i++) {
                // printf("%s\t", inet_ntoa(*(struct in_addr*)&runner_server->next->peers[i]));
                // printf("%s\n", inet_ntoa(*(struct in_addr*)&client_IP));//For temp test
                if (runner_server->next->peers[i] == client_IP) {
                    peer_exist = 1;
                    break;
                }
            }
            if (peer_exist) {
                /* If peer's ip exists in the file node, we delete the file node from server table */
                printf("server should delete %s\n", runner_server->next->name);
                /* Delete the file node from server table */
                file_node *temp = runner_server->next;
                runner_server->next = temp->next;
                free(temp);
            } else {
                /* If peer's ip doesn't exist in the file node, we ask client to add this file */
                printf("client should add %s\n", runner_server->next->name);
            }
        }
        runner_server = runner_server->next;
    }
    // printf("file table in the middle::::::::::::::::\n");
    // file_table_print();
    file_node* runner_client = client_table;
    while (runner_client->next != NULL) {
        // printf("reach .. %s\n", runner_client->next->name);
        // file_table_print();
        runner_server = file_table;
        int is_exist = 0;
        while (runner_server->next != NULL) {
            if (strcmp(runner_client->next->name, runner_server->next->name) == 0) {
                is_exist = 1;
                break;
            }
            runner_server = runner_server->next;
        }
        if (is_exist == 0) {
            /* If file exists in client table, but not in server table */
            printf("server should add %s\n", runner_client->next->name);
            // /* Add the file node to server table */
            file_node *temp = (file_node*)malloc(sizeof(file_node));
            bzero(temp, sizeof(file_node));
            temp->size = runner_client->next->size;
            sprintf(temp->name, "%s", runner_client->next->name);
            temp->timestamp = runner_client->next->timestamp;
            temp->num_peers = runner_client->next->num_peers;
            temp->peers[0] = runner_client->next->peers[0];
            /* Add to the server table list */
            temp->next = file_table->next;
            file_table->next = temp;
        } else {			
            if (runner_client->next->timestamp < runner_server->next->timestamp) {
                printf("client should update %s\n", runner_client->next->name);
            } else if (runner_client->next->timestamp > runner_server->next->timestamp) {
                printf("server should update %s\n", runner_client->next->name);
                runner_server->next->size = runner_client->next->size;
                runner_server->next->timestamp = runner_client->next->timestamp;
                runner_server->next->num_peers = runner_client->next->num_peers;
                runner_server->next->peers[0] = runner_client->next->peers[0];
            } else { 
                /* Update the peers in the file node */
                int i;
                int peer_exist = 0;
                for (i = 0; i < runner_server->next->num_peers; i++) {
                    if (runner_server->next->peers[i] == client_IP) {
                        peer_exist = 1;
                        break;
                    }
                }
                if (peer_exist == 0) {
                    runner_server->next->peers[runner_server->next->num_peers] = client_IP;
                    runner_server->next->num_peers++;
                }
            }
        }
        runner_client = runner_client->next;
    }
}

/* Used by server to delete disconnected peer from file table */
void delete_disconn_peer(unsigned long client_IP) {
    file_node* runner_server = file_table;
    while (runner_server->next != NULL) {
        int i;
        int peer_exist = 0;
        for (i = 0; i < runner_server->next->num_peers; i++) {
            if (runner_server->next->peers[i] == client_IP) {
                peer_exist = 1;
                break;
            }
        }
        runner_server->next->num_peers--;
        runner_server->next->peers[i] = runner_server->next->peers[runner_server->next->num_peers];
        runner_server = runner_server->next;
    }
}

/* Used by server to broad the updated file table to all the peers */
void broadcast_file_table() { 
    tracker_peer_t *runner = get_peer_table();
    while (runner->next != NULL) {
        printf("Broadcast to %s\n", inet_ntoa(*(struct in_addr*)&runner->next->ip));
        send_file_table(runner->next->sockfd);
        runner = runner->next;
    }
}