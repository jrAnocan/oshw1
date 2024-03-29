#include "logging.h"
#include "message.h"
#include <iostream>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <poll.h>
#include <cstring>
#include <sstream>
#include <cmath>

using namespace std;

#define READ_END 0
#define WRITE_END 1
#define MAX_SEE_DISTANCE 3

typedef struct bomber
{
    coordinate c;
    vector<string> args;
    bool marked = false;
    bool die_message_recieved = false;
    bool ready = false;
    double last_distance = -1;
    bool win = false;
} bomber;

typedef struct bomb
{
    coordinate c;
    pid_t pid;
    int fd[2];
    struct pollfd poll;
    int radius;
    bool ready = false;
} bomb;

int bomber_count;
bool winner_informed = false;
int reaped_bombers = 0;

void getObstacles(vector<obsd> *&obstacles, int obstacle_count, vector<vector<string>> &lines)
{
    for (int i = 0; i < obstacle_count; i++)
    {
        obsd o;
        o.position.x = stoi(lines[1 + i][0]);
        o.position.y = stoi(lines[1 + i][1]);
        o.remaining_durability = stoi(lines[1 + i][2]);
        obstacles->push_back(o);
    }
}

void closeOtherPipes(int pipes[][2], int n, int bomber_count)
{
    int i;
    for (i = 0; i < bomber_count; i++)
    {
        if (i != n)
        {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
    }
}

int bomberThere(vector<bomber> &bombers, coordinate check_coordinate)
{
    int res = -1;
    for (int i = 0; i < bombers.size(); i++)
    {
        if (bombers[i].c.x == check_coordinate.x && bombers[i].c.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}

int bombThere(vector<bomb> &bombs, coordinate c)
{
    int res = -1;

    for (int i = 0; i < bombs.size(); i++)
    {
        if (bombs[i].c.x == c.x && bombs[i].c.y == c.y)
        {
            return i;
        }
    }
    return res;
}

int obstacleThere(vector<obsd> *&obstacles, coordinate check_coordinate)
{
    int res = -1;
    for (int i = 0; i < (*obstacles).size(); i++)
    {
        if ((*obstacles)[i].position.x == check_coordinate.x && (*obstacles)[i].position.y == check_coordinate.y)
        {
            return i;
        }
    }
    return res;
}

void checkDirection(vector<obsd>*& obstacles, vector<bomb>& bombs, vector<bomber>& bombers, vector<vector<int>>& res, coordinate initial_coordinate, bool dir_left, bool dir_right, bool dir_up, bool dir_down
                   , int width, int height)
{
    int initial = 1;

    if (dir_down || dir_left)
    {
        initial = -1;
    }
    

    coordinate check_coordinate;
    check_coordinate.x = initial_coordinate.x; // first check the horizontals
    check_coordinate.y = initial_coordinate.y;

    if(!(dir_left || dir_right || dir_up || dir_down)) //check self -- only for bomb
    {
        int bomb_index = bombThere(bombs, check_coordinate);
        if ((bomb_index >= 0))
        {
            res[1].push_back(bomb_index);
            
        }
        return;
    }


    for (int i = initial; i*initial <= 3 * initial*initial; i+=initial)
    {

        

        if (dir_right || dir_left)
        {
            check_coordinate.x = initial_coordinate.x + i; // first check the horizontals
            
            if(check_coordinate.x<0 || check_coordinate.x >= width)
            {
                break;
            }

        }
        else if (dir_up || dir_down)
        {
            
            check_coordinate.y = initial_coordinate.y + i;
            if(check_coordinate.y<0 || check_coordinate.y >= height)
            {
                break;
            }
    
        }
        
        int bomber_index = bomberThere(bombers, check_coordinate);
        int bomb_index = bombThere(bombs, check_coordinate);
        int obstacle_index = obstacleThere(obstacles, check_coordinate);
        if ((bomber_index >= 0))
        {
            res[0].push_back(bomber_index);
        }

        if ((bomb_index >= 0))
        {
            res[1].push_back(bomb_index);
            
        }
        /*
            cout<<"current coordinate: "<<check_coordinate.x<<","<<check_coordinate.y<<endl;
            for(int i=0;i<bombs.size();i++)
            {
                
                cout<<"bomb "<<i<<" "<<bombs[i].c.x<<","<<bombs[i].c.y<<endl;

            }
        
        */
        if ((obstacle_index >= 0))
        {
            res[2].push_back(obstacle_index);
            break;
        }

        
    }
}

vector<vector<int>> indexesOfNearObjects(vector<bomber> &bombers, vector<bomb> &bombs, vector<obsd> *&obstacles, int n, int width, int height)
{
    // BOMBER + BOMB + OBSTACLE

    vector<vector<int>> res;
    for (int i = 0; i < 3; i++)
    {
        res.push_back(vector<int>());
    }

    coordinate initial_coordinate = bombers[n].c;
    checkDirection(obstacles, bombs, bombers, res, initial_coordinate, false, false, false, false, width, height);  // self
    checkDirection(obstacles, bombs, bombers, res, initial_coordinate, true, false, false, false, width, height);  // left
    checkDirection(obstacles, bombs, bombers, res, initial_coordinate, false, true, false, false, width, height);  // right
    checkDirection(obstacles, bombs, bombers, res, initial_coordinate, false, false, true, false, width, height);  // up
    checkDirection(obstacles, bombs, bombers, res, initial_coordinate, false, false, false, true, width, height);  // down


    return res;
}

bool newCoordinateAvailable(coordinate current, coordinate target, int width, int height)
{
    if (target.x >= width - 1 || target.y >= height - 1)
    {
        return false;
    }
    if (target.x == current.x && (abs((int)target.y - (int)current.y) == 1))
    {
        return true;
    }
    if (target.y == current.y && abs((int)target.x - (int)current.x) == 1)
    {
        return true;
    }
    return false;
}

int getBombIndex(vector<bomb> &bombs, int pid)
{

    for (int i = 0; i < bombs.size(); i++)
    {
        if (pid == bombs[i].pid)
        {
            return i;
        }
    }
    return -1;
}

bool obstacleBetween(vector<obsd> *&obstacles, coordinate bomber_position, coordinate explosion_origin)
{
    bool same_line_x;
    bool same_line_y;
    if (bomber_position.y == explosion_origin.y)
    {
        same_line_x = true;
    }
    if (bomber_position.x == explosion_origin.x)
    {
        same_line_y = true;
    }
    if ((same_line_x && same_line_y) || (!same_line_x && !same_line_y)) // on the bomb || on different lines
    {
        return false;
    }
    else if (same_line_x && !same_line_y)
    {
        for (int i = 0; i < (*obstacles).size(); i++)
        {
            coordinate obstacle_coordinate = (*obstacles)[i].position;
            int min_x = min(bomber_position.x, explosion_origin.x);
            int max_x = max(bomber_position.x, explosion_origin.x);
            if ((obstacle_coordinate.y == bomber_position.y) &&
                (min_x < obstacle_coordinate.x) &&
                (obstacle_coordinate.x < max_x))
            {
                return true;
            }
        }
    }
    else if (same_line_y && !same_line_x)
    {
        for (int i = 0; i < (*obstacles).size(); i++)
        {
            coordinate obstacle_coordinate = (*obstacles)[i].position;
            int min_y = min(bomber_position.y, explosion_origin.y);
            int max_y = max(bomber_position.y, explosion_origin.y);
            if ((obstacle_coordinate.x == bomber_position.x) &&
                (min_y < obstacle_coordinate.y) &&
                (obstacle_coordinate.y < max_y))
            {
                return true;
            }
        }
    }

    return false;
}

bool caughtInExplosion(vector<obsd> *&obstacles, coordinate object_position, coordinate explosion_origin, int radius, object_type type)
{
    bool res = false;
    switch (type)
    {
    case OBSTACLE:
    {
        if (object_position.x == explosion_origin.x && abs((int)object_position.y - (int)explosion_origin.y) <= radius)
        {
            res = true;
        }
        else if (object_position.y == explosion_origin.y && abs((int)object_position.x - (int)explosion_origin.x) <= radius)
        {
            res = true;
        }
        break;
    }
    case BOMBER:
    {
        if (obstacleBetween(obstacles, object_position, explosion_origin))
        {
            res = false;
        }
        else
        {
            if (object_position.x == explosion_origin.x && abs((int)object_position.y - (int)explosion_origin.y) <= radius)
            {
                res = true;
                bomber_count--;
            }
            else if (object_position.y == explosion_origin.y && abs((int)object_position.x - (int)explosion_origin.x) <= radius)
            {
                res = true;
                bomber_count--;
            }
        }

        break;
    }
    default:
        break;
    }
    return res;
}

void decreaseDurabilityOfObstacles(vector<obsd> *&obstacles, coordinate c, int radius)
{
    vector<obsd> damaged_obstacles;
    for (int i = 0; i < obstacles->size(); i++)
    {
        if (caughtInExplosion(obstacles, (*obstacles)[i].position, c, radius, OBSTACLE))
        {
            int durability = (*obstacles)[i].remaining_durability;
            if (durability == -1)
            {
                damaged_obstacles.push_back((*obstacles)[i]);
                continue;
            }
            else if (durability == 1)
            {
                (*obstacles)[i].remaining_durability--;
                damaged_obstacles.push_back((*obstacles)[i]);
                (*obstacles).erase((*obstacles).begin() + i);
            }
            else
            {
                (*obstacles)[i].remaining_durability--;
                damaged_obstacles.push_back((*obstacles)[i]);
            }
        }
    }
    if (damaged_obstacles.size() > 0)
    {
        obsd print_obs[damaged_obstacles.size()];
        for (int i = 0; i < damaged_obstacles.size(); i++)
        {
            print_obs[i] = damaged_obstacles[i];
        }
        print_output(NULL, NULL, print_obs, NULL);
    }
}

void informMarkedBomber(vector<bomber> &bombers, int pipes[][2], int pid_table[], int n)
{
    int status;
    om *message_out = new om;

    omp *message_out_print = new omp;
    message_out->type = BOMBER_DIE;

    message_out_print->pid = pid_table[n];
    message_out_print->m = message_out;

    send_message(pipes[n][WRITE_END], message_out);
    print_output(NULL, message_out_print, NULL, NULL);

    close(pipes[n][WRITE_END]);

    delete message_out;
    delete message_out_print;
    bombers[n].die_message_recieved = true;

    waitpid(pid_table[n], &status, 0);
    reaped_bombers++;
}

void informWinnerBomber(vector<bomber> &bombers, int pipes[][2], int pid_table[], int n)
{
    int status;
    om *message_out = new om;

    omp *message_out_print = new omp;
    message_out->type = BOMBER_WIN;

    message_out_print->pid = pid_table[n];
    message_out_print->m = message_out;

    send_message(pipes[n][WRITE_END], message_out);
    print_output(NULL, message_out_print, NULL, NULL);

    close(pipes[n][WRITE_END]);

    delete message_out;
    delete message_out_print;
    winner_informed = true;
    waitpid(pid_table[n], &status, 0);
    reaped_bombers++;
}

void serveBomberMessage(imp *&im, vector<bomber> &bombers, vector<bomb> &bombs, int n, int pipes[][2], int pid_table[], vector<obsd> *&obstacles, int width, int height)
{
    switch (im->m->type)
    {
    case BOMBER_START:
    {
        if (bomber_count == 1)
        {

            informWinnerBomber(bombers, pipes, pid_table, n);
            return;
        }

        om *message_out = new om;
        omd *message_out_data = new omd;
        omp *message_out_print = new omp;
        message_out_data->new_position = (bombers)[n].c;
        message_out->data = *(message_out_data);
        message_out->type = BOMBER_LOCATION;
        message_out_print->m = message_out;
        message_out_print->pid = im->pid;
        send_message(pipes[n][WRITE_END], message_out);
        print_output(NULL, message_out_print, NULL, NULL);
        delete message_out;
        delete message_out_data;
        delete message_out_print;
        break;
    }
    case BOMBER_MOVE:
    {
        if (bombers[n].marked)
        {
            informMarkedBomber(bombers, pipes, pid_table, n);
            return;
        }
        if (bombers[n].win)
        {
            informWinnerBomber(bombers, pipes, pid_table, n);
            return;
        }

        om *message_out = new om;
        omd *message_out_data = new omd;
        omp *message_out_print = new omp;

        coordinate current_pos, target_pos, new_pos;
        current_pos = bombers[n].c;
        target_pos = im->m->data.target_position;
        if (newCoordinateAvailable(current_pos, target_pos, width, height))
        {
            new_pos = im->m->data.target_position;
            bombers[n].c = new_pos;
        }
        else
        {
            new_pos = current_pos;
        }
        message_out_data->new_position = new_pos;

        message_out->data = *(message_out_data);
        message_out->type = BOMBER_LOCATION;
        message_out_print->m = message_out;
        message_out_print->pid = im->pid;
        send_message(pipes[n][WRITE_END], message_out);
        print_output(NULL, message_out_print, NULL, NULL);
        delete message_out;
        delete message_out_data;
        delete message_out_print;
        break;
    }
    case BOMBER_PLANT:
    {
        if (bombers[n].marked)
        {
            informMarkedBomber(bombers, pipes, pid_table, n);
            return;
        }
        if (bombers[n].win)
        {
            informWinnerBomber(bombers, pipes, pid_table, n);
            return;
        }
        coordinate bomber_location;
        int bomber_pid = im->pid;
        int bomber_index;
        for (bomber_index = 0; bomber_index < bombers.size(); bomber_index++)
        {
            if (pid_table[bomber_index] == bomber_pid)
            {
                break;
            }
        }
        //----------------->didnt check if pid exists in pid table. Assume it would.
        bomber_location = bombers[bomber_index].c;
        int is_bomb_there = bombThere(bombs, bomber_location) + 1; ///-----------------------------------------> IMPORTANt. bombThere returns -1 if false
        if (!is_bomb_there)                                        // No bomb ---> plant
        {
            int fd[2];
            PIPE(fd);

            bomb b;
            // cout<<"bomb"<<" "<<fd[READ_END]<<fd[WRITE_END]<<endl;

            b.c = bomber_location;
            b.fd[0] = fd[0];
            b.fd[1] = fd[1];
            b.radius = im->m->data.bomb_info.radius;
            int pid = fork();

            if (pid)
            {
                close(fd[READ_END]);
                // cout<<"CONTROLLER"<<fd[0]<<"--------"<<fd[1]<<endl;
                om *message_out = new om;
                omd *message_out_data = new omd;
                omp *message_out_print = new omp;

                b.pid = pid;

                struct pollfd poll;

                bombs.push_back(b);
                // close(bombs[bombs.size()-1].fd[READ_END]); //-------------------------------------------

                message_out_data->planted = 1;
                message_out->data = *(message_out_data);
                message_out->type = BOMBER_PLANT_RESULT;
                message_out_print->m = message_out;
                message_out_print->pid = im->pid;
                send_message(pipes[n][WRITE_END], message_out);
                print_output(NULL, message_out_print, NULL, NULL);

                memset(&poll, 0, sizeof(poll));
                poll.fd = fd[WRITE_END];
                poll.events = POLLIN;
                bombs[bombs.size() - 1].poll = poll;
                delete message_out;
                delete message_out_data;
                delete message_out_print;
            }
            else
            {

                // cout<<"BOMB"<<fd[0]<<"--------"<<fd[1]<<endl;
                close(fd[WRITE_END]);
                close(fileno(stdin)); // bomb stdin close?

                dup2(fd[READ_END], fileno(stdout));
                const char *arg = to_string(im->m->data.bomb_info.interval).c_str();
                execl("./bomb", "./bomb", arg, NULL);
            }
        }

        else // Bomb ---> no plant
        {
            om *message_out = new om;
            omd *message_out_data = new omd;
            omp *message_out_print = new omp;
            message_out_data->planted = 0;
            message_out->data = *(message_out_data);
            message_out->type = BOMBER_PLANT_RESULT;
            message_out_print->m = message_out;
            message_out_print->pid = im->pid;
            send_message(pipes[n][WRITE_END], message_out);
            print_output(NULL, message_out_print, NULL, NULL);
            delete message_out;
            delete message_out_data;
            delete message_out_print;
        }

        break;
    }

    case BOMB_EXPLODE:
    {

        for (int bomber_index = 0; bomber_index < bombers.size(); bomber_index++)
        {
            if (!bombers[bomber_index].marked && !bombers[bomber_index].win && caughtInExplosion(obstacles, bombers[bomber_index].c, bombs[n].c, bombs[n].radius, BOMBER))
            {
                bombers[bomber_index].marked = true;
                bombers[bomber_index].last_distance = sqrt(pow(bombers[bomber_index].c.x - bombs[n].c.x, 2) + pow(bombers[bomber_index].c.y - bombs[n].c.y, 2));
            }
        }

        if (bomber_count == 1)
        {
            for (int bomber_index = 0; bomber_index < bombers.size(); bomber_index++)
            {
                if (!bombers[bomber_index].marked && !bombers[bomber_index].die_message_recieved)
                {
                    bombers[bomber_index].win = true;
                    break;
                }
            }
        }

        if (bomber_count == 0)
        {
            int highest_index = -1;
            double highest_distance = -1;
            for (int bomber_index = 0; bomber_index < bombers.size(); bomber_index++)
            {
                if (!bombers[bomber_index].die_message_recieved)
                {

                    if (bombers[bomber_index].last_distance > highest_distance) //>= necessary? i think not.
                    {
                        highest_distance = bombers[bomber_index].last_distance;
                        highest_index = bomber_index;
                    }
                }
            }
            bombers[highest_index].marked = false;
            bombers[highest_index].win = true;
        }

        decreaseDurabilityOfObstacles(obstacles, bombs[n].c, bombs[n].radius);
        break;
    }

    case BOMBER_SEE:
    {
        if (bombers[n].marked)
        {
            informMarkedBomber(bombers, pipes, pid_table, n);
            return;
        }

        if (bombers[n].win)
        {
            informWinnerBomber(bombers, pipes, pid_table, n);
            return;
        }

        vector<vector<int>> near_objects = indexesOfNearObjects(bombers, bombs, obstacles, n, width, height);

        int object_count = near_objects[0].size() + near_objects[1].size() /*+ near_objects[2].size()*/; // BOMBER + BOMB + OBSTACLE
        od objects[object_count];                                                                        // objects in an array

        om *message_out = new om;
        omd *message_out_data = new omd;
        omp *message_out_print = new omp;

        int ind = 0;

        for (int i = 0; i < 3; i++) // 3 types of objects --> 0 -> bomber, 1->bomb 2 -> obstacle
        {
            for (int j = 0; j < near_objects[i].size(); j++)
            {
                od obj;
                switch (i)
                {
                case 0: // bomber
                {
                    obj.type = BOMBER;
                    obj.position = bombers[near_objects[i][j]].c;
                    break;
                }

                case 1: // bomb
                {
                    obj.type = BOMB;
                    obj.position = bombs[near_objects[i][j]].c;
                    break;
                }
                /*
                case 2: // obstacle
                {
                    obj.type = OBSTACLE;
                    obj.position = (*obstacles)[near_objects[i][j]].position;
                    break;
                }
                */
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
        message_out_print->pid = im->pid;
        //cout<<"n is: "<<n<<endl;
        //cout << "pid table entry is: "<<pid_table[n]<<endl;
        send_message(pipes[n][WRITE_END], message_out);
        send_object_data(pipes[n][WRITE_END], object_count, objects);
        print_output(NULL, message_out_print, NULL, objects);

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
    int line_no = 0;
    while (getline(cin, l))
    {
        if (l.empty())
        {
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
    width = stoi(lines[0][0]);
    height = stoi(lines[0][1]);
    obstacle_count = stoi(lines[0][2]);
    bomber_count = stoi(lines[0][3]);

    vector<obsd> *obstacles = new vector<obsd>;

    getObstacles(obstacles, obstacle_count, lines);

    int pipes[bomber_count][2];
    struct pollfd polls[bomber_count];

    int pid_table[bomber_count] = {};

    vector<bomb> bombs;

    vector<bomber> bombers;

    int offset = 0;

    for (int i = 0; i < line_no - 1 - obstacle_count; i += 2)
    {
        bomber b;
        b.c.x = stoi(lines[obstacle_count + 1 + i][0]);
        b.c.y = stoi(lines[obstacle_count + 1 + i][1]);
        b.args = lines[obstacle_count + 2 + i];
        b.marked = false;
        b.die_message_recieved = false;
        bombers.push_back(b);
    }

    for (int i = 0; i < bomber_count; i++)
    {
        PIPE(pipes[i]);
    }

    for (int i = 0; i < bomber_count; i++)
    {
        int pid = fork();

        if (pid) // parent --controller
        {
            pid_table[i] = pid;
            close(pipes[i][READ_END]);
            memset(&(polls[i]), 0, sizeof(polls[i]));
            polls[i].fd = pipes[i][1];
            polls[i].events = POLLIN;
        }
        else // child --bomber
        {

            close(pipes[i][WRITE_END]);

            dup2(pipes[i][READ_END], fileno(stdin));
            dup2(pipes[i][READ_END], fileno(stdout));

            char *inp[bombers[i].args.size() + 1];

            int j;
            for (j = 0; j < bombers[i].args.size(); j++)
            {
                char *c = new char[bombers[i].args[j].length() + 1];
                copy(bombers[i].args[j].begin(), bombers[i].args[j].end(), c);
                inp[j] = c;
            }
            inp[j] = NULL;

            execv(inp[0], inp);
        }
    }

    /*
    for(int i=0;i<bomber_count;i++)
    {
        cout<<"pid_table: "<<i<<" has id: "<<pid_table[i]<<endl;
    }
    */

    while (bomber_count > 1)
    {
        for (int i = 0; i < bombs.size(); i++)
        {
            if (poll(&bombs[i].poll, 1, 0))
            {
                bombs[i].ready = true;
            }
        }

        int size = bombs.size();
        for (int i = 0; i < size; i++)
        {
            if (bombs[i].ready)
            {
                im *m = new im;
                imp *mp = new imp;
                read_data(bombs[i].fd[WRITE_END], m);

                close(bombs[i].fd[WRITE_END]);

                mp->m = m;
                mp->pid = bombs[i].pid;

                print_output(mp, NULL, NULL, NULL);

                if (bomber_count > 1)
                    serveBomberMessage(mp, bombers, bombs, i, pipes, pid_table, obstacles, width, height);
                else
                    decreaseDurabilityOfObstacles(obstacles, bombs[i].c, bombs[i].radius);

                delete m;
                delete mp;
                waitpid(bombs[i].pid, &status, 0);
                bombs.erase(bombs.begin() + i);
                i--;
                size--;
            }
        }

        if (bomber_count <= 1)
        {
            break;
        }

        int ret_bomber = poll(polls, sizeof(pipes) / sizeof(pipes[0]), 0);

        if (ret_bomber)
        {
            for (int i = 0; i < sizeof(pipes) / sizeof(pipes[0]); i++)
            {

                if (polls[i].revents & POLLIN && !bombers[i].die_message_recieved)
                {

                    im *m = new im;
                    imp *mp = new imp;
                    read_data(pipes[i][WRITE_END], m);
                    mp->m = m;
                    mp->pid = pid_table[i];

                    print_output(mp, NULL, NULL, NULL);
                    serveBomberMessage(mp, bombers, bombs, i, pipes, pid_table, obstacles, width, height);

                    delete m;
                    delete mp;
                }
            }
        }

        sleep(0.001);
    }

    // handleBombs(obstacles, bombs, bombers, pipes, pid_table, width, height);

    int in = 0;
    int bomber_left = bomber_count;

    while (reaped_bombers < bombers.size())
    {
        int ret_bomber = poll(polls, sizeof(pipes) / sizeof(pipes[0]), 0);
        if (ret_bomber)
        {
            for (int i = 0; i < sizeof(pipes) / sizeof(pipes[0]); i++)
            {

                if (polls[i].revents & POLLIN && !bombers[i].die_message_recieved)
                {

                    im *m = new im;
                    imp *mp = new imp;
                    read_data(pipes[i][WRITE_END], m);
                    mp->m = m;
                    mp->pid = pid_table[i];

                    print_output(mp, NULL, NULL, NULL);
                    serveBomberMessage(mp, bombers, bombs, i, pipes, pid_table, obstacles, width, height);

                    delete m;
                    delete mp;
                }
            }
        }
    }

    while (bombs.size() > 0)
    {

        for (int i = 0; i < bombs.size(); i++)
        {
            if (poll(&bombs[i].poll, 1, 0))
            {
                bombs[i].ready = true;
            }
        }

        int size = bombs.size();
        for (int i = 0; i < size; i++)
        {
            if (bombs[i].ready)
            {
                im *m = new im;
                imp *mp = new imp;
                read_data(bombs[i].fd[WRITE_END], m);

                close(bombs[i].fd[WRITE_END]);

                mp->m = m;
                mp->pid = bombs[i].pid;

                print_output(mp, NULL, NULL, NULL);

                if (bomber_count > 1)
                    serveBomberMessage(mp, bombers, bombs, i, pipes, pid_table, obstacles, width, height);
                else
                    decreaseDurabilityOfObstacles(obstacles, bombs[i].c, bombs[i].radius);

                delete m;
                delete mp;
                
                waitpid(bombs[i].pid, &status, 0);

                bombs.erase(bombs.begin() + i);
                i--;
                size--;
            }
        }
    }

    for (int i = 0; i < bombs.size(); i++)
    {
        close(bombs[i].fd[WRITE_END]);
    }
    for (int i = 0; i < bombers.size(); i++)
    {

        close(pipes[i][WRITE_END]);
    }
    for (int i = 0; i < bomber_count; i++)
    {

        waitpid(pid_table[i], &status, 0);
    }

    return 0;
}
