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

int main(int argc, char *argv[])
{
    vector<string> allArgs(argv, argv+argc); // first is ./bgame. Start reading from second.


    int width, height, obstacle_count, bomber_count, status;
    width = stoi(allArgs[1]); height = stoi(allArgs[2]); obstacle_count = stoi(allArgs[3]); bomber_count = stoi(allArgs[4]);
    
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
            
            //char* arr[] = {"ls", "-l", "-R", "-a", NULL};
            //execv("/bin/ls", arr);
            
            execv(inp[0],inp);
            
            
           
        }
    }
    /*
    for(int i=0;i<bomber_count;i++)
    {
        cout<<pid_table[i]<<" ";
    }
    */
   
   for(int i=0; i<bomber_count;i++) // create pipes
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
                bomber_count--;
                
                /*
                char buf[1024];
                read(pipes[i][1],buf,1024);
                cout<<buf<<endl;
                bomber_count--;
                */
            }

        }
        
    }
    
    
    
    


    
  
    for(int i=0;i<bomber_count;i++)
    {
        waitpid(pid_table[i], &status, 0);
    }

    return 0;

}
