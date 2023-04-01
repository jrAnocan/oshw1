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
    bool marked;
    bool die_message_recieved;
} bomber;

typedef struct bomb
{
    coordinate c;
    pid_t pid;
    int fd[2];
    struct pollfd poll;
    int radius;
} bomb;

int bomber_count;

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
    int res=-1;
    for(int i=0;i<bombers.size();i++)
    {
        if(bombers[i].c.x == check_coordinate.x && bombers[i].c.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}

int bombThere(vector<bomb>& bombs, coordinate c)
{
    int res=-1;
    
    for(int i=0;i<bombs.size();i++)
    {
        if(bombs[i].c.x == c.x && bombs[i].c.y == c.y)
        {
            return i;
        }
    }
    return res;
}

int obstacleThere(vector<obsd>*& obstacles, coordinate check_coordinate)
{
    int res=-1;
    for(int i=0;i<(*obstacles).size();i++)
    {
        if((*obstacles)[i].position.x == check_coordinate.x && (*obstacles)[i].position.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}


vector<vector<int>> indexesOfNearObjects(vector<bomber>& bombers, vector<bomb>& bombs, vector<obsd>*& obstacles, int n, int width, int height)
{
    // BOMBER + BOMB + OBSTACLE   

    


    vector<vector<int>> res;
    for(int i=0;i<3;i++)
    {
        res.push_back(vector<int>());
    }

    coordinate initial_coordinate = bombers[n].c;
    /*
    for(int i=0;i<bombs.size();i++)
    {
        cout<<"current coordinate: "<<initial_coordinate.x<<","<<initial_coordinate.y<<endl;
        cout<<"bomb "<<i<<" "<<bombs[i].c.x<<","<<bombs[i].c.y<<endl;
    }
    */

    for(int i=-3;i<=3;i++)
    {
        int offset = abs(i);
        
        for(int j=-3+offset;j<=3-offset;j++)
        {
            
            
            
            
            coordinate check_coordinate;

            check_coordinate.x = initial_coordinate.x + j; // first check the horizontals
            check_coordinate.y = initial_coordinate.y + i; 

            int bomber_index = bomberThere(bombers, check_coordinate);
            int bomb_index = bombThere(bombs, check_coordinate);
            int obstacle_index = obstacleThere(obstacles,check_coordinate);
            if((bomber_index>=0) && i!=0 && j!=0)
            {
                res[0].push_back(bomber_index);
            }
            
             if((bomb_index>=0))
            {
                res[1].push_back(bomb_index);
            }
            /*
            if(i==0 && j==0)
            {
                for(int i=0;i<bombs.size();i++)
                {
                    cout<<"current coordinate: "<<check_coordinate.x<<","<<check_coordinate.y<<endl;
                    cout<<"bomb "<<i<<" "<<bombs[i].c.x<<","<<bombs[i].c.y<<endl;
                    
                }
            }
            */
             if((obstacle_index>=0))
            {
                res[2].push_back(obstacle_index);
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

int getBombIndex(vector<bomb>& bombs, int pid)
{
    
    for(int i=0;i<bombs.size();i++)
    {
        if(pid == bombs[i].pid)
        {
            return i;
        }
    }
    return -1;
}

bool obstacleBetween(vector<obsd>*& obstacles, coordinate bomber_position, coordinate explosion_origin)
{
    bool same_line_x;
    bool same_line_y;
    if(bomber_position.y == explosion_origin.y) {same_line_x = true;}
    if(bomber_position.x == explosion_origin.x) {same_line_y = true;}    
    if((same_line_x && same_line_y) || (!same_line_x && !same_line_y)) // on the bomb || on different lines
    {
        return false;
    }
    else if(same_line_x && !same_line_y)
    {
        for(int i=0;i<(*obstacles).size();i++)
        {
            coordinate obstacle_coordinate = (*obstacles)[i].position;
            int min_x = min(bomber_position.x, explosion_origin.x); int max_x = max(bomber_position.x, explosion_origin.x);
            if( (obstacle_coordinate.y == bomber_position.y) && 
                (min_x < obstacle_coordinate.x) &&
                (obstacle_coordinate.x < max_x)  )
            {
                return true;
            }
               
        }
    }
    else if(same_line_y && !same_line_x)
    {
        for(int i=0;i<(*obstacles).size();i++)
        {
            coordinate obstacle_coordinate = (*obstacles)[i].position;
            int min_y = min(bomber_position.y, explosion_origin.y); int max_y = max(bomber_position.y, explosion_origin.y);
            if( (obstacle_coordinate.x == bomber_position.x) && 
                (min_y < obstacle_coordinate.y) &&
                (obstacle_coordinate.y < max_y)  )
            {
                return true;
            }
               
        }
    }

    return false;
}


bool caughtInExplosion(vector<obsd>*& obstacles, coordinate object_position, coordinate explosion_origin, int radius, object_type type)
{
    bool res = false;
    switch (type)
    {
    case OBSTACLE:
    {
        if(object_position.x == explosion_origin.x && abs((int) object_position.y - (int) explosion_origin.y)<=radius)
        {
            res = true;
        }
        else if(object_position.y == explosion_origin.y && abs((int) object_position.x - (int) explosion_origin.x)<=radius)
        {
            res = true;
        }
        break;
    }
    case BOMBER:
    {
        if(obstacleBetween(obstacles, object_position, explosion_origin))
        {
            res = false;
        }
        else
        {
            if(object_position.x == explosion_origin.x && abs((int) object_position.y - (int) explosion_origin.y)<=radius)
            {
                res = true;
            }
            else if(object_position.y == explosion_origin.y && abs((int) object_position.x - (int) explosion_origin.x)<=radius)
            {
                res = true;
            }
        }

        break;
    }   
    default:
        break;
    }
    return res;
}

void decreaseDurabilityOfObstacles(vector<obsd>*& obstacles, coordinate c, int radius)
{
    for(int i=0;i<obstacles->size();i++)
    {
        if(caughtInExplosion(obstacles, (*obstacles)[i].position, c, radius, OBSTACLE))
        {
            int durability = (*obstacles)[i].remaining_durability;
            if( durability== -1)
            {
                continue;
            }
            else if(durability == 1)
            {
               (*obstacles).erase((*obstacles).begin() + i); 
            }
            else
            {
                (*obstacles)[i].remaining_durability--;
            }
        }
    }
}

void informMarkedBomber(vector<bomber>& bombers, int pipes[][2], int pid_table[], int n )
{
    om* message_out = new om;
    
    omp* message_out_print = new omp;
    message_out->type = BOMBER_DIE;
    
    message_out_print->pid = pid_table[n];
    message_out_print->m = message_out;

    send_message(pipes[n][WRITE_END], message_out);
    print_output(NULL,message_out_print,NULL,NULL);
                        
    close(pipes[n][WRITE_END]);
    bomber_count--;
    delete message_out;   
    delete message_out_print;

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
        if(bombers[n].marked )
        {
           informMarkedBomber(bombers, pipes, pid_table, n);
           return;
        }

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
        if(bombers[n].marked )
        {
           informMarkedBomber(bombers, pipes, pid_table, n);
           return;
        }

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
        int is_bomb_there = bombThere(bombs, bomber_location)+1; ///-----------------------------------------> IMPORTANt. bombThere returns -1 if false
        if(!is_bomb_there) // No bomb ---> plant
        {
            int fd[2];
            PIPE(fd);
            
            bomb b;
            //cout<<"bomb"<<" "<<fd[READ_END]<<fd[WRITE_END]<<endl;
            
            b.c = bomber_location;
            b.fd[0] = fd[0]; b.fd[1] = fd[1];
            b.radius = im->m->data.bomb_info.radius;
            
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
        
        int bomb_index = getBombIndex(bombs,im->pid);    
        if(bomb_index==-1)
        {
            perror("bomb_index is -1");
            return;
        }
        
        if(caughtInExplosion(obstacles, bombers[n].c,bombs[bomb_index].c,bombs[bomb_index].radius,BOMBER))
        {
            bombers[n].marked = true;

        }
        decreaseDurabilityOfObstacles(obstacles, bombs[bomb_index].c,bombs[bomb_index].radius );
        break;
    }

    case BOMBER_SEE:
    {
        if(bombers[n].marked )
        {
           informMarkedBomber(bombers, pipes, pid_table, n);
           return;
        }

        


        vector<vector<int>> near_objects = indexesOfNearObjects(bombers, bombs, obstacles, n, width, height);
    
        int object_count = near_objects[0].size() + near_objects[1].size() + near_objects[2].size(); // BOMBER + BOMB + OBSTACLE  
        od objects[object_count];  //objects in an array
        
        om* message_out = new om;
        omd* message_out_data = new omd;
        omp* message_out_print = new omp;
        
        int ind=0;
        
        
        
        for(int i=0;i<3;i++)  // 3 types of objects --> 0 -> bomber, 1->bomb 2 -> obstacle
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
                
                case 1: //bomb
                {
                    obj.type = BOMB;
                    obj.position = bombs[near_objects[i][j]].c;
                    break;
                }   

                case 2: //obstacle
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
  
    
    int width, height, obstacle_count, status;
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
        b.marked = false;
        b.die_message_recieved = false;
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

        int ret_bomb = poll(bomb_polls,size,0);
        
        
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
        
        
        

        int ret_bomber = poll(polls,bomber_count,0);
        
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
    
    for(int i=0;i<bombers.size();i++)
    {
        
        
            close(pipes[i][WRITE_END]);
        
    }
    for(int i=0;i<bombs.size();i++)
    {
        close(bombs[i].fd[WRITE_END]);
    }
    for(int i=0;i<bomber_count;i++)
    {
        if(bombers[i].die_message_recieved)
        {
            waitpid(pid_table[i], &status, 0);
        }
        
    }
    
    return 0;

}
