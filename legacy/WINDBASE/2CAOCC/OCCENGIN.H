void battery_condition(uint16);
int  check_input(TARGET *temp);

void toggle(void);
void input_targetNO(TARGET *temp);

void center_win(char *, int);
char *cord_2_string(int, int);
void decide_battery(BYTE,int);
void decide_battery2(BYTE battery,/*	BYTE battery27,BYTE battery76,*/int y);

void delete_course(int batt);
void moving_battery(void);
void moving(void);
uint16 calc_distance(int batt);

int insert_target(TARGET *temp);
int user_fire(TARGET *);
void user_shoot(void);
int calc_gmet(TARGET *);
uint16 check_rfl(TARGET *target);
void command_fire(void);
void fire(TARGET *target);
void show_battery(void);
void show_bat_tan(void);
void show_fire_result(FIRE *temp);
void fire_result_store(TARGET *target, int choice_battery, int sort);
void fire_result(void);
void check_target_battery(int battery);
void check_target(TARGET *, int *, int *, int);
void update_target(TARGET *dest,TARGET *sour);
uint16 target_list(int,BYTE);
TARGET *target_search(void);
TARGET *seek_target(char *targetNO);
TARGET *target_seek(int xcord, int ycord);
void show_target_information(TARGET *target);
int effect(BYTE effectsort);

void firearms_resource(int chapt);
void show_firearms_resource(void);

int menu(void);

uint16 check = TRUE;            //  for moving battery  unputimaging....
uint16 move_situation = TRUE;  //  for moving road drawing...
int change_check;
int base_color = YELLOW, base_bkcolor = LIGHTBLUE;

void target_input(void);
void init_target(TARGET *temp);
void target_store(TARGET *info);


uint16 response(char *, uint16 y_n);


#define CHOICE 100

float DEGREE = 0.05625;

uint16 SITUATION_BOARD = TRUE;

//union REGS d;
struct dosdate_t dosday;
struct date D;
struct time T;

int TARGET_VOL = 0;
int SECTOR_VOL = 6;

int OLD_SECTOR_VOL = 4;

int GRID_WIDTH = 84;                  //67
int OLD_GRID_WIDTH = 100;

int LEFT_LIMIT  = 7500;
int RIGHT_LIMIT = 10500;
int UPPER_LIMIT = 13000;
int LOWER_LIMIT = 10000;
int fire_minus;
uint16 TIME_TOGGLE = TRUE;
int RANGE_TOGGLE = 1;




void fileload(void);
void filesave(uint16);
void file_list(int,int,int);
void fileerase(int);
//////////////////////////////////
void fsave(uint16 size);