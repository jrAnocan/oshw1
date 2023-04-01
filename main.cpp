#include "logging.h"
#include "message.h"
#include<iostream>
#include <stdlib.h>
#include <string>
#include<vector>
#include <sys/wait.h> 
#include <poll.h>
#include <cstring>
#include <sstream>

using namespace std;

#define READ_END 0
#define WRITE_END 1
#define MAX_SEE_DISTANCE 3

typedef struct bomber
{
    coordinate c;
    vector<string> args;
} bomber;

typedef struct bomb
{
    coordinate c;
    pid_t pid;
    int fd[2];
    struct pollfd poll;
} bomb;


void getObstacles(vector<obsd>*& obstacles, int obstacle_count, vector<vector<string>>& lines)
{
    for(int i=0;i<obstacle_count;i++)
    {
        obsd o;
        o.position.x = stoi(lines[1+i][0]);
        o.position.y = stoi(lines[1+i][1]);
        o.remaining_durability = stoi(lines[1+i][2]);
        obstacles->push_back(o);
    }

}

void closeOtherPipes(int pipes[][2], int n, int bomber_count)
{
    int i;
    for(i=0;i<bomber_count;i++)
    {
        if(i!=n)
        {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
    }
}


int bomberThere(vector<bomber>& bombers, coordinate check_coordinate)
{
    int res=0;
    for(int i=0;i<bombers.size();i++)
    {
        if(bombers[i].c.x == check_coordinate.x && bombers[i].c.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}

int obstacleThere(vector<obsd>*& obstacles, coordinate check_coordinate)
{
    int res=0;
    for(int i=0;i<(*obstacles).size();i++)
    {
        if((*obstacles)[i].position.x == check_coordinate.x && (*obstacles)[i].position.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}


vector<vector<int>> indexesOfNearObjects(vector<bomber>& bombers, vector<obsd>*& obstacles, int n, int width, int height)
{
    // BOMBER + OBSTACLE   
    vector<vector<int>> res;
    coordinate initial_coordinate = bombers[n].c;
    for(int i=-3;i<=3;i++)
    {
        int offset = abs(i);
        res.push_back(vector<int>());
        for(int j=-3+offset;j<=3-offset;j++)
        {
            
            if(i==0 && j==0) continue; //exclude self
            if(i>width-1 || j > height-1) continue; // exclude out of borders
            coordinate check_coordinate;

            check_coordinate.x = initial_coordinate.x + j; // first check the horizontals
            check_coordinate.y = initial_coordinate.y + i; 

            int bomber_index = bomberThere(bombers, check_coordinate);
            int obstacle_index = obstacleThere(obstacles,check_coordinate);
            if(bomber_index)
            {
                res[0].push_back(bomber_index);
            }
            
            else if(obstacle_index)
            {
                res[1].push_back(obstacle_index);
            }
        }
    }
    return res;
}

bool newCoordinateAvailable(coordinate current, coordinate target, int width, int height)
{
    if(target.x >= width-1 || target.y >=height-1)
    {
        return false;
    }
    if(target.x == current.x && (abs((int) target.y - (int)current.y)==1) )
    {
        return true;
    }
    if(target.y == current.y && abs((int) target.x - (int) current.x)==1)
    {
        return true;
    }
    return false;
}

int bombThere(vector<bomb>& bombs, coordinate c)
{
    for(int i=0;i<bombs.size();i++)
    {
        if(bombs[i].c.x == c.x && bombs[i].c.y == c.y)
        {
            return 1;
        }
    }
    return 0;
}

void serveBomberMessage(imp*& im, vector<bomber>& bombers, vector<bomb>& bombs, int n, int pipes[][2], int pid_table[]
                        , vector<obsd>*& obstacles, int width, int height)
{
    switch (im->m->type)
    {
    case BOMBER_START:
    {
        om* message_out = new om;
        omd* message_out_data = new omd;
        omp* message_out_print = new omp;
        message_out_data->new_position = (bombers)[n].c;
        message_out -> data = *(message_out_data);
        message_out ->type = BOMBER_LOCATION;
        message_out_print->m = message_out;
        message_out_print ->pid = pid_table[n]; 
        send_message(pipes[n][WRITE_END], message_out);
        print_output(NULL,message_out_print,NULL,NULL);
        delete message_out;
        delete message_out_data;
        delete message_out_print;
        break;
    }
    case BOMBER_MOVE:
    {
        om* message_out = new om;
        omd* message_out_data = new omd;
        omp* message_out_print = new omp;

        coordinate current_pos, target_pos, new_pos;
        current_pos = bombers[n].c;
        target_pos = im->m->data.target_position;
        if(newCoordinateAvailable( current_pos, target_pos, width, height))
        {
            new_pos = im->m->data.target_position;
            bombers[n].c = new_pos;
        }
        else
        {
            new_pos = current_pos;
        }
        message_out_data->new_position = new_pos;

        message_out -> data = *(message_out_data);
        message_out ->type = BOMBER_LOCATION;
        message_out_print->m = message_out;
        message_out_print ->pid = pid_table[n]; 
        send_message(pipes[n][WRITE_END], message_out);
        print_output(NULL,message_out_print,NULL,NULL);
        delete message_out;
        delete message_out_data;
        delete message_out_print;
        break;
        
    }
    case BOMBER_PLANT:
    {
    
        coordinate bomber_location ;
        int bomber_pid = im->pid;
        int bomber_index;
        for(bomber_index=0;bomber_index<bombers.size();bomber_index++)
        {
            if(pid_table[bomber_index] == bomber_pid)
            {
                break;
            }
        }    
                  //----------------->didnt check if pid exists in pid table. Assume it would.
        bomber_location = bombers[bomber_index].c;
        int is_bomb_there = bombThere(bombs, bomber_location);
        if(!is_bomb_there) // No bomb ---> plant
        {
            int fd[2];
            PIPE(fd);
            
            bomb b;
            //cout<<"bomb"<<" "<<fd[READ_END]<<fd[WRITE_END]<<endl;
            
            b.c = bomber_location;
            b.fd[0] = fd[0]; b.fd[1] = fd[1];
            
            
            int pid = fork();
            
            if(pid)
            {
                close(fd[READ_END]);
                //cout<<"CONTROLLER"<<fd[0]<<"--------"<<fd[1]<<endl;
                om* message_out = new om;
                omd* message_out_data = new omd;
                omp* message_out_print = new omp;

                
                b.pid = pid;

                struct pollfd poll;
                

                bombs.push_back(b);
                //close(bombs[bombs.size()-1].fd[READ_END]); //-------------------------------------------
                
                message_out_data->planted = 1;
                message_out->data = *(message_out_data);
                message_out->type = BOMBER_PLANT_RESULT;
                message_out_print->m = message_out;
                message_out_print->pid = pid_table[n];
                send_message(pipes[n][WRITE_END], message_out);
                print_output(NULL,message_out_print,NULL,NULL);

                memset(&poll, 0,sizeof(poll));
                poll.fd = fd[WRITE_END];
                poll.events = POLLIN;
                bombs[bombs.size()-1].poll = poll;
                delete message_out;
                delete message_out_data;
                delete message_out_print;
                

            }
            else
            {
                
                //cout<<"BOMB"<<fd[0]<<"--------"<<fd[1]<<endl;
                close(fd[WRITE_END]);
                close(fileno(stdin)); //bomb stdin close?
                
                dup2(fd[READ_END],fileno(stdout));
                const char* arg = to_string(im->m->data.bomb_info.interval).c_str();
                execl("./bomb","./bomb",arg,NULL);
                
            }

            
        }
            
        else //Bomb ---> no plant
        {
            om* message_out = new om;
            omd* message_out_data = new omd;
            omp* message_out_print = new omp;
            message_out_data->planted = 0;
            message_out->data = *(message_out_data);
            message_out->type = BOMBER_PLANT_RESULT;
            message_out_print->m = message_out;
            message_out_print->pid = pid_table[n];
            send_message(pipes[n][WRITE_END], message_out);
            print_output(NULL,message_out_print,NULL,NULL);
            delete message_out;
            delete message_out_data;
            delete message_out_print;
            
        }
            
        
        
        
        
    
        break;
    }
    
    case BOMB_EXPLODE:
    {
        /*
        int bomb_index;
        int status;
        for(bomb_index=0;bomb_index<bombs.size();bomb_index++)
        {
            if(im->pid == bombs[bomb_index].pid)
            {
                break;
            }
        }

        waitpid(bombs[bomb_index].pid,&status,0);
        */
        break;
    }

    case BOMBER_SEE:
    {
        vector<vector<int>> near_objects = indexesOfNearObjects(bombers, obstacles, n, width, height);
    
        int object_count = near_objects[0].size() + near_objects[1].size() + near_objects[2].size(); // BOMBER + BOMB + OBSTACLE  
        
        od objects[object_count];  //objects in an array
        
        om* message_out = new om;
        omd* message_out_data = new omd;
        omp* message_out_print = new omp;
        
        int ind=0;
        for(int i=0;i<2;i++)  // 2 types of objects --> 0 -> bomber, 1 -> obstacle
        {
            for(int j=0;j<near_objects[i].size();j++) 
            {
                od obj;
                switch (i)
                {
                case 0:  //bomber
                {
                    obj.type = BOMBER;
                    obj.position = bombers[near_objects[i][j]].c;
                    break;
                }
                
                case 1: //obstacle
                {
                    obj.type = OBSTACLE;
                    obj.position = (*obstacles)[near_objects[i][j]].position;
                    break;
                }   
                
                default:
                    break;
                } 
                objects[ind] = obj;
                ind++;
            }
        }
        
        
        message_out->type = BOMBER_VISION;
        message_out_data->object_count = object_count;
        message_out->data = *(message_out_data);
        message_out_print->m = message_out;
        message_out_print ->pid = pid_table[n]; 

        

        send_message(pipes[n][WRITE_END], message_out);
        send_object_data(pipes[n][WRITE_END], object_count, objects);
        print_output(NULL,message_out_print,NULL,objects);

        delete message_out;
        delete message_out_data;
        delete message_out_print;
        
        
        break;
    }

    default:
        break;
    }
}

bool bombExploded(int pid, vector<int>& exploded_bomb_pids)
{
    for(int i=0;i<exploded_bomb_pids.size();i++)
    {
        if(pid == exploded_bomb_pids[i])
        {
            return true;
        }
    }
    return false;
}


int main()
{
    string l;
    vector<vector<string>> lines;
    int line_no=0;
    while (getline(cin, l))
    {
        if (l.empty()) {
            break;
        }
        lines.push_back(vector<string>());
        
        istringstream ss(l);
 
        string word; 
        while (ss >> word)
        {
       
            lines[line_no].push_back(word);
        }
        line_no++;
    }
  
    
    int width, height, obstacle_count, bomber_count, status;
    width = stoi(lines[0][0]); height = stoi(lines[0][1]); obstacle_count = stoi(lines[0][2]); bomber_count = stoi(lines[0][3]);
    
    vector<obsd>* obstacles = new vector<obsd>;
      
    getObstacles(obstacles,obstacle_count,lines);

    int pipes[bomber_count][2];
    struct pollfd polls[bomber_count];
    
    int pid_table[bomber_count] = { };

    vector<bomb> bombs;
    


    vector<bomber> bombers ;



    int offset = 0;
    
    for(int i=0; i<line_no-1-obstacle_count ; i+=2)
    {
        bomber b;
        b.c.x = stoi(lines[obstacle_count+1+i][0]); 
        b.c.y = stoi(lines[obstacle_count+1+i][1]); 
        b.args = lines[obstacle_count+2+i];
        bombers.push_back(b);
    }
    
    
    

    for(int i=0;i<bomber_count;i++)
    {
        PIPE(pipes[i]);
        int pid = fork();
        pid_table[i]=pid;

        if(pid) //parent --controller
        {
            
            close(pipes[i][READ_END]);
            memset(&(polls[i]), 0, sizeof(polls[i]));
            polls[i].fd = pipes[i][1];
            polls[i].events = POLLIN;
            
            
        }
        else //child --bomber
        {
                
            close(pipes[i][WRITE_END]);
            
            dup2(pipes[i][READ_END],fileno(stdin));
            dup2(pipes[i][READ_END],fileno(stdout));

            char* inp[bombers[i].args.size()+1];
            
            int j;
            for(j=0;j<bombers[i].args.size();j++)
            {       
                char* c = new char[bombers[i].args[j].length() + 1];        
                copy(bombers[i].args[j].begin(), bombers[i].args[j].end(), c);        
                inp[j] = c;
            }
            inp[j]=NULL;  
                   
            execv(inp[0],inp);      
        }
    }
    
    while(bomber_count > 1)
    {
        
        int size = bombs.size();
        struct pollfd bomb_polls[size];
        for(int i=0;i<size;i++)
        {
            bomb_polls[i] = bombs[i].poll;
        }

        int ret_bomb = poll(bomb_polls,size,100);
        
        
        if(ret_bomb)
        {
            for(int i=0;i<size;i++)
            {
                if(bomb_polls[i].revents & POLLIN)
                {
                    im* m = new im;
                    imp* mp = new imp;
                    read_data(bombs[i].fd[WRITE_END], m);
                    
                    close(bombs[i].fd[WRITE_END]);

                    mp->m = m;
                    mp->pid = bombs[i].pid;
                   
                    print_output(mp,NULL,NULL,NULL);
                    
                    serveBomberMessage(mp,bombers,bombs,i,pipes, pid_table, obstacles, width, height);
                    bomb_polls[i].revents = 0;
                    delete m;
                    delete mp;


                    waitpid(bombs[i].pid,&status,0);
                    bombs.erase(bombs.begin() + i);
                    i--; size--; 
                }
            }
        }
        
        
        

        int ret_bomber = poll(polls,bomber_count,10);
        
        if(ret_bomber) // &&!ret_bomb
        {
            for(int i=0;i<sizeof(pipes)/sizeof(pipes[0]);i++)
            {
                

                if(polls[i].revents & POLLIN )
                {
                    
                    im* m = new im;
                    imp* mp = new imp;
                    read_data(pipes[i][WRITE_END], m);
                    mp->m = m;
                    mp->pid = pid_table[i];
                    

                    print_output(mp,NULL,NULL,NULL);
                    serveBomberMessage(mp,bombers,bombs,i,pipes, pid_table, obstacles, width, height);
                    
                    delete m;
                    delete mp;
                    
                }

            }
        }
        
        sleep(0.001);  
    }
  
    for(int i=0;i<bomber_count;i++)
    {
        waitpid(pid_table[i], &status, 0);
    }
    
    return 0;

}
