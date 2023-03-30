#include "logging.h"
#include "message.h"
#include<iostream>
#include <stdlib.h>
#include <string>
#include<vector>
#include <sys/wait.h> 
#include <poll.h>
#include <cstring>

using namespace std;

#define READ_END 0
#define WRITE_END 1
#define MAX_SEE_DISTANCE 3

typedef struct bomber
{
    coordinate c;
    vector<string> args;
} bomber;


void getObstacles(vector<obsd>*& obstacles, int obstacle_count, char* argv[])
{
    for(int i=0;i<obstacles->size();i++)
    {
        obsd o;
        o.position.x = stoi(argv[5+3*i]);
        o.position.y = stoi(argv[6+3*i]);
        o.remaining_durability = stoi(argv[7+3*i]);
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


void serveBomberMessage(imp*& im, vector<bomber>& bombers, int n, int pipes[][2], int pid_table[]
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
        break;
    }
    case BOMBER_PLANT:
    {

        break;
    }
    
    case BOMB_EXPLODE:
    {

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


int main(int argc, char *argv[])
{
    
    int width, height, obstacle_count, bomber_count, status;
    width = stoi(argv[1]); height = stoi(argv[2]); obstacle_count = stoi(argv[3]); bomber_count = stoi(argv[4]);
    
    vector<obsd>* obstacles = new vector<obsd>;
      
    getObstacles(obstacles,obstacle_count,argv);

    int pipes[bomber_count][2];
    struct pollfd polls[bomber_count];
    
    int pid_table[bomber_count] = { };

    
    vector<bomber> bombers ;
            
    int offset = 0;
    
    for(int i=0; i<bomber_count ; i++)
    {
        bomber b;
        int ind = 5+3*obstacle_count+offset;
        int arg_count = stoi(argv[ind+2]);
        b.c.x = stoi(argv[ind]); b.c.y = stoi(argv[ind+1]);
        int j;
        for(j=0; j<arg_count; j++)
        {
            b.args.push_back( argv[ind+3+j] );
        }
        
        b.args.push_back("\0");
        offset += 2+arg_count+1;
        bombers.push_back(b);
 
    }

   
    
    for(int i=0; i<bomber_count;i++) // create pipes
    {
        PIPE(pipes[i]);
        
    }

    for(int i=0;i<bomber_count;i++)
    {
        int pid = fork();
        pid_table[i]=pid;

        if(pid) //parent --controller
        {
            
            close(pipes[i][READ_END]);
            
            
            
            
        }
        else //child --bomber
        {
            
            
            close(pipes[i][WRITE_END]);

            dup2(pipes[i][READ_END],fileno(stdin));
            dup2(pipes[i][READ_END],fileno(stdout));

            char* inp[bombers[i].args.size()];
            
            int j;
            for(j=0;j<bombers[i].args.size()-1;j++)
            {
               
                char* c = new char[bombers[i].args[j].length() + 1];

                
                
                copy(bombers[i].args[j].begin(), bombers[i].args[j].end(), c);
                
                inp[j] = c;
                //cout << inp[j] << endl;
                
            }
            inp[j]=NULL;          
            execv(inp[0],inp);      
        }
    }
    /*
    for(int i=0;i<bomber_count;i++)
    {
        cout<<pid_table[i]<<" ";
    }
    */
   
   for(int i=0; i<bomber_count;i++) 
    {
        
        memset(&(polls[i]), 0, sizeof(polls[i]));
        polls[i].fd = pipes[i][1];
        polls[i].events = POLLIN;

    }
        
    while(bomber_count > 0)
    {
        for(int i=0;i<sizeof(pipes)/sizeof(pipes[1]);i++)
        {
            
            

            if(poll(&(polls[i]), 1, 100) > 0)
            {
                
                im* m = new im;
                imp* mp = new imp;
                read_data(pipes[i][WRITE_END], m);
                mp->m = m;
                mp->pid = pid_table[i];
                print_output(mp,NULL,NULL,NULL);
                serveBomberMessage(mp,bombers,i,pipes,pid_table, obstacles, width, height);
                delete m;
                delete mp;
                
                
            }

        }
        
    }
  
    for(int i=0;i<bomber_count;i++)
    {
        waitpid(pid_table[i], &status, 0);
    }

    return 0;

}
