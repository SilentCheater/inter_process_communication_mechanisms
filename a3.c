#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int shm_fd;
char* shared_chars = NULL;

int mmf_fd;
char* mm_file;
int mm_size;


int main(int argc, char** argv){

    // pipe - based connection
    if(mkfifo("./RESP_PIPE_30817", 0600) < 0)
    {
        printf("ERROR\n cannot create the response pipe");
        return -1;
    }

    int fd_req = open("./REQ_PIPE_30817", O_RDONLY);
    if(fd_req < 0)
    {
        printf("ERROR\n cannot open the request pipe");
        return -1;
    }

    int fd_resp = open("./RESP_PIPE_30817", O_WRONLY);
    if(fd_resp < 0)
    {
        printf("ERROR\n cannot open the response pipe");
        return -1;
    }

    int response1 = write(fd_resp, "\x07\x43\x4f\x4e\x4e\x45\x43\x54", 8);

    if(response1 < 0)
    {
        printf("ERROR\n cannot write to the response pipe");
        return -1;
    }

    if(response1 == 8)
    {
        //printf("SUCCESS\n");
    }

    while(1)
    {
        int req_size;
        char req_size_char;
        if(read(fd_req, &req_size_char, 1) != 1)
        {
            printf("ERROR\n");
            return -1;
        }
        req_size = (int) req_size_char;

        switch (req_size)
        {
        case 4:
        {
            char req[5];
            if(read(fd_req, req, 4) != 4)
            {
                printf("ERROR\n");
                return -1;
            }
            req[4]='\0';

            // handle case "PING"
            if(!strcmp(req, "PING"))
            {
                // write the response on the pipe
                if(write(fd_resp, "\x04\x50\x49\x4e\x47\x04\x50\x4f\x4e\x47\x61\x78\x00\x00", 14) < 14)
                {
                    printf("ping error\n");
                    return -1;
                }
            }
            //  handle case EXIT
            else if (!strcmp(req, "EXIT"))
            {
                close(fd_req);
                close(fd_resp);
                close(mmf_fd);
                munmap(shared_chars, 3505044);
                munmap(mm_file, mm_size);
                return 0;
            }
            break;
        }

        // SHM REQUEST
        case 10:
        {
            char buffcshm[15];
            if(read(fd_req, buffcshm, 14) == 14)
            {
                buffcshm[14]='\0';
                char cshm[15] = "\x43\x52\x45\x41\x54\x45\x5f\x53\x48\x4d\x94\x7b\x35\x00\0";
                if(!strcmp(buffcshm, cshm))
                {
                    shm_fd = shm_open("/QBNRDG7", O_CREAT | O_RDWR, 0664);
                    if(shm_fd < 0)
                    {
                        printf("Could not aquire shm\n");
                        write(fd_resp, "\x0a\x43\x52\x45\x41\x54\x45\x5f\x53\x48\x4d\x05\x45\x52\x52\x4f\x52", 17);
                        return -1;
                    }

                    ftruncate(shm_fd, 3505044);

                    shared_chars = (char*)mmap(0, 3505044, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
                    if(shared_chars == MAP_FAILED)
                    {
                        printf("Could not map the shared memory\n");
                        write(fd_resp, "\x0a\x43\x52\x45\x41\x54\x45\x5f\x53\x48\x4d\x05\x45\x52\x52\x4f\x52", 17);

                        return -1;
                    }
                    close(shm_fd);

                    if(write(fd_resp, "\x0a\x43\x52\x45\x41\x54\x45\x5f\x53\x48\x4d\x07\x53\x55\x43\x43\x45\x53\x53", 19) != 19)
                    {
                        return -1;
                    }
                }
            }
            break;

        }

        case 12:
        {
            char write_req[21];
            if(read(fd_req, write_req, 20) == 20)
            {
                write_req[20]='\0';

                if(strncmp(write_req, "WRITE_TO_SHM", 12) == 0)
                {
                    unsigned char off[4] = {write_req[12], write_req[13], write_req[14], write_req[15]};
                    int offset = off[0] | ( off[1]<<8) | ( off[2]<<16) | ( off[3]<<24);

                    if(offset+4 <= 3505044 && offset >= 0)
                    {
                        shared_chars[offset]  = write_req[16];
                        shared_chars[offset+1]  = write_req[17];
                        shared_chars[offset+2]  = write_req[18];
                        shared_chars[offset+3]  = write_req[19];
                        write(fd_resp, "\x0c\x57\x52\x49\x54\x45\x5f\x54\x4f\x5f\x53\x48\x4d\x07\x53\x55\x43\x43\x45\x53\x53",21);
                    }
                    else
                        write(fd_resp, "\x0c\x57\x52\x49\x54\x45\x5f\x54\x4f\x5f\x53\x48\x4d\x05\x45\x52\x52\x4f\x52", 19);
                }
            }
            break;
        }

        case 8:
        {
            char map_req[9];
            read(fd_req, map_req, 8);

            if(strncmp(map_req, "MAP_FILE", 8) == 0)
            {
                //read the size of the name of the file
                int file_name_size_int;
                char file_name_size_char;
                if(read(fd_req, &file_name_size_char, 1) != 1)
                {
                    printf("ERROR\n");
                    return -1;
                }

                // convert char to int
                file_name_size_int = (int) file_name_size_char;

                char file_name[file_name_size_int+1];

                // read the name of the file
                read(fd_req, file_name, file_name_size_int);

                file_name[file_name_size_int]='\0';

                mmf_fd = open(file_name, O_RDONLY);
                if(mmf_fd == -1)
                {
                    printf("ERROR\n");
                    write(fd_resp, "\x08\x4d\x41\x50\x5f\x46\x49\x4c\x45\x05\x45\x52\x52\x4f\x52", 15);
                    return -1;
                }
                mm_size = lseek(mmf_fd, 0, SEEK_END);
                lseek(mmf_fd, 0, SEEK_SET);
                mm_file = (char*) mmap(NULL, mm_size, PROT_READ, MAP_PRIVATE ,mmf_fd, 0);
                if(mm_file == MAP_FAILED)
                {
                    printf("ERROR\n");
                    write(fd_resp, "\x08\x4d\x41\x50\x5f\x46\x49\x4c\x45\x05\x45\x52\x52\x4f\x52", 15);
                    return -1;
                }
                write(fd_resp, "\x08\x4d\x41\x50\x5f\x46\x49\x4c\x45\x07\x53\x55\x43\x43\x45\x53\x53", 17);
            }
            break;
        }

        case 21: 
        {
            char rffo[21];
            read(fd_req, rffo, 21);
            if(strncmp(rffo, "READ_FROM_FILE_OFFSET", 21) == 0)
            {
                int offset, bytes;
                char off[4];
                read(fd_req, off, 4);
                offset = (unsigned char)off[0] | ( (unsigned char)off[1]<<8) | ( (unsigned char)off[2]<<16) | ( (unsigned char)off[3]<<24);
                
                
                for(int i = 0; i<4;i++)
                {
                    printf("%d: %hhx\n", i, ((unsigned char*)&offset)[i]);
                    printf("%d: %hhx\n", i, off[i]);
                }
                printf("off: %d\n", offset);

                char byt[4];
                read(fd_req, byt, 4);

                bytes = (unsigned char)byt[0] | ( (unsigned char)byt[1]<<8) | ( (unsigned char)byt[2]<<16) | ( (unsigned char)byt[3]<<24);
 
                printf("bytes: %d\n", bytes);
                for(int i = 0; i<4;i++)
                {
                    printf("%d: %hhx\n", i, ((unsigned char*)&bytes)[i]);
                }
                printf("mm_size: %d\n", mm_size); 

                if(offset+bytes <= mm_size && shared_chars != MAP_FAILED && mm_file != MAP_FAILED)
                {
                   for(int i = 0; i < bytes; i++)
                    {
                        shared_chars[i] = mm_file[i+offset];
                    }
                    write(fd_resp, "\x15\x52\x45\x41\x44\x5f\x46\x52\x4f\x4d\x5f\x46\x49\x4c\x45\x5f\x4f\x46\x46\x53\x45\x54\x07\x53\x55\x43\x43\x45\x53\x53", 30); 
                }
                else
                    write(fd_resp, "\x15\x52\x45\x41\x44\x5f\x46\x52\x4f\x4d\x5f\x46\x49\x4c\x45\x5f\x4f\x46\x46\x53\x45\x54\x05\x45\x52\x52\x4f\x52", 28); 

            }
            break;
        }

        case 22: 
        {
            char rffs[22];
            read(fd_req, rffs, 22);
            if(strncmp(rffs, "READ_FROM_FILE_SECTION", 22) == 0)
            {
                int section, offset, bytes;
                char sect[4];
                read(fd_req, sect, 4);
                section = (unsigned char)sect[0] | ( (unsigned char)sect[1]<<8) | ( (unsigned char)sect[2]<<16) | ( (unsigned char)sect[3]<<24);

                char off[4];
                read(fd_req, off, 4);
                offset = (unsigned char)off[0] | ( (unsigned char)off[1]<<8) | ( (unsigned char)off[2]<<16) | ( (unsigned char)off[3]<<24);

                char byt[4];
                read(fd_req, byt, 4);
                bytes = (unsigned char)byt[0] | ( (unsigned char)byt[1]<<8) | ( (unsigned char)byt[2]<<16) | ( (unsigned char)byt[3]<<24);

                // declarations
                char magic[4];
                unsigned char header_size[2];
                unsigned char version[2];
                unsigned char nr_of_sections_char;

                //magic
                for(int i = 0; i<4;i++)
                    magic[i]=mm_file[mm_size-4+i];

                //header size
                header_size[0]=mm_file[mm_size-6];
                header_size[1]=mm_file[mm_size-5];
                int header_size_int = header_size[0] | header_size[1] | 0;

                //version
                version[0]=mm_file[mm_size-header_size_int];
                version[1]=mm_file[mm_size-header_size_int+1];
                int version_nr = version[0] | (version[1]<<8) | 0;

                //nr of sections
                nr_of_sections_char = mm_file[mm_size-header_size_int+2];
                int nr_of_sections = nr_of_sections_char | 0;

                unsigned char sect_type[2];
                unsigned char sect_offset[4];
                unsigned char sect_size[4];


                sect_type[0]=mm_file[mm_size - header_size_int + 3 + (21*(section-1)) + 11];       
                sect_type[1]=mm_file[mm_size - header_size_int + 3 + (21*(section-1)) + 11 + 1];         
                int section_type = sect_type[0] | (sect_type[1]<<8);

                if(
                    strncmp(magic, "NxNs", 4) == 0 &&
                    (version_nr <= 70 && version_nr >= 27) &&
                    (nr_of_sections <= 17 && nr_of_sections >= 4) &&
                    (section_type == 92 || section_type == 68 || section_type == 49 || section_type == 72 || section_type == 61 || section_type == 42)
                )
                {
                    for (int j = 0; j< 4;j++)
                    {
                        sect_offset[j]=mm_file[mm_size - header_size_int + 3 + (21*(section-1)) + 11 + 2 + j];
                        sect_size[j] = mm_file[mm_size - header_size_int + 3 + (21*(section-1)) + 11 + 2 + 4 + j];
                    }

                    int sect_size_int, sect_offset_int;
                    sect_offset_int = sect_offset[0] | ( sect_offset[1]<<8) | ( sect_offset[2]<<16) | ( sect_offset[3]<<24);
                    sect_size_int = sect_size[0] | ( sect_size[1]<<8) | ( sect_size[2]<<16) | ( sect_size[3]<<24);

                    if(offset + bytes <= sect_size_int)
                        for(int i = 0; i < bytes; i++)
                        {
                            shared_chars[i] = mm_file[sect_offset_int + offset +i];
                        }
                    write(fd_resp, "\x16\x52\x45\x41\x44\x5f\x46\x52\x4f\x4d\x5f\x46\x49\x4c\x45\x5f\x53\x45\x43\x54\x49\x4f\x4e\x07\x53\x55\x43\x43\x45\x53\x53", 31);
                }
                else
                write(fd_resp, "\x16\x52\x45\x41\x44\x5f\x46\x52\x4f\x4d\x5f\x46\x49\x4c\x45\x5f\x53\x45\x43\x54\x49\x4f\x4e\x05\x45\x52\x52\x4f\x52", 29);

            }

            break;
        }
            
        default:
        {
            close(fd_req);
            close(fd_resp);
            close(mmf_fd);
            return 0;
            break;
        }
        }
    }

    return 0;

}