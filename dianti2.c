#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <linux/input.h>
#include "../DRMwrap.h"

// 全局变量
struct drmHandle drm;
int fb;      // 屏幕文件
int touch;   // 触摸文件

// 状态变量
int floor_flag[7] = {0}; // 1-6楼是否被按下
int current_floor = 1;   // 当前楼层
int door_state = 0;      // 0关，1开
int door_timer = 0;      // 倒计时

// ================== 1. 显示图片函数 ==================
// 最基础的显示函数，用于瞬间显示
int bmp_show(char *name, int start_x, int start_y) {
    int bmp_fb = open(name, O_RDONLY);
    if(bmp_fb == -1) return -1;
    
    char buf[54];
    read(bmp_fb, buf, 54);
    int w = *(int *)&buf[18];
    int h = *(int *)&buf[22];
    
    char *bmp_buf = (char *)malloc(w*h*3);
    read(bmp_fb, bmp_buf, w*h*3);

    char *share = drm.vaddr;
    
    for(int x = 0; x < w; x++){
        for(int y = 0; y < h; y++){
            int real_x = start_x + x;
            int real_y = start_y + y;
            if(real_x >= 1024 || real_y >= 600) continue;
            
            int pos = 4 * (1024 * real_y + real_x);
            int bmp_pos = 3 * (w * (h - 1 - y) + x);
            
            share[pos+0] = bmp_buf[bmp_pos+0];
            share[pos+1] = bmp_buf[bmp_pos+1];
            share[pos+2] = bmp_buf[bmp_pos+2];
            share[pos+3] = 0;
        }
    }
    free(bmp_buf);
    close(bmp_fb);
    return 0;
}

// 带特效的显示函数 (模仿zhsx.c，每20列刷新一次)
int bmp_effect(char *name, int start_x, int start_y) {
    int bmp_fb = open(name, O_RDONLY);
    if(bmp_fb == -1) return -1;
    char buf[54]; read(bmp_fb, buf, 54);
    int w = *(int *)&buf[18]; int h = *(int *)&buf[22];
    char *bmp_buf = (char *)malloc(w*h*3);
    read(bmp_fb, bmp_buf, w*h*3);
    
    char *share = drm.vaddr;
    for(int x = 0; x < w; x++){
        for(int y = 0; y < h; y++){
            int real_x = start_x + x; int real_y = start_y + y;
            int pos = 4 * (1024 * real_y + real_x);
            int bmp_pos = 3 * (w * (h - 1 - y) + x);
            share[pos+0] = bmp_buf[bmp_pos+0]; 
            share[pos+1] = bmp_buf[bmp_pos+1]; 
            share[pos+2] = bmp_buf[bmp_pos+2]; 
            share[pos+3] = 0;
        }
        if(x % 20 == 0) { 
            DRMshowUp(fb, &drm); 
            usleep(2000); 
        }
    }
    DRMshowUp(fb, &drm); 
    free(bmp_buf); close(bmp_fb);
    return 0;
}

// ================== 2. 界面刷新函数 ==================

// 刷新所有按钮
void button_show(){
    // --- 1楼 ---
    if(floor_flag[1] == 1) bmp_show("liang1.bmp", 60, 350);
    else                   bmp_show("anniu1.bmp", 60, 350);
    
    // --- 2楼 ---
    if(floor_flag[2] == 1) bmp_show("liang2.bmp", 260, 350);
    else                   bmp_show("anniu2.bmp", 260, 350);
    
    // --- 3楼 ---
    if(floor_flag[3] == 1) bmp_show("liang3.bmp", 460, 350);
    else                   bmp_show("anniu3.bmp", 460, 350);
    
    // --- 4楼 ---
    if(floor_flag[4] == 1) bmp_show("liang4.bmp", 60, 480);
    else                   bmp_show("anniu4.bmp", 60, 480);
    
    // --- 5楼 ---
    if(floor_flag[5] == 1) bmp_show("liang5.bmp", 260, 480);
    else                   bmp_show("anniu5.bmp", 260, 480);
    
    // --- 6楼 ---
    if(floor_flag[6] == 1) bmp_show("liang6.bmp", 460, 480);
    else                   bmp_show("anniu6.bmp", 460, 480);
    
    // --- 开关门 ---
    bmp_show("kai.bmp", 800, 350);
    bmp_show("guan.bmp", 800, 480);
    
    DRMshowUp(fb, &drm);
}

// 刷新门的状态条 (绿开红关)
void door_show(){
    char *share = drm.vaddr;
    int r = (door_state == 0) ? 255 : 0;
    int g = (door_state == 1) ? 255 : 0;
    
    for(int y = 280; y < 300; y++){
        for(int x = 0; x < 1024; x++){
            int pos = 4*(1024*y+x);
            share[pos+0] = 0; 
            share[pos+1] = g; 
            share[pos+2] = r; 
            share[pos+3] = 0;
        }
    }
    DRMshowUp(fb, &drm);
}

// 画箭头 (1上 2下)
void arrow_show(int type){
    char *share = drm.vaddr;
    // 涂黑箭头区域
    for(int y = 50; y < 150; y++) 
        for(int x = 600; x < 700; x++)
            { int p=4*(1024*y+x); share[p]=0; share[p+1]=0; share[p+2]=0; share[p+3]=0; }
    
    if(type == 1){ // 向上(红)
        int i = 0;
        for(int y = 50; y < 90; y++){
            for(int x = 650 - i; x <= 650 + i; x++){
                int p=4*(1024*y+x); share[p]=0; share[p+1]=0; share[p+2]=255; share[p+3]=0;
            }
            i++;
        }
        for(int y = 90; y < 130; y++) // 矩形柄
            for(int x = 640; x < 660; x++)
                { int p=4*(1024*y+x); 
                    share[p]=0; 
                    share[p+1]=0; 
                    share[p+2]=255; 
                    share[p+3]=0; }
    } 
    else { // 向下(绿)
        for(int y = 50; y < 90; y++) // 矩形柄
            for(int x = 640; x < 660; x++)
                { int p=4*(1024*y+x); share[p]=0; share[p+1]=255; share[p+2]=0; share[p+3]=0; }
        int i = 40;
        for(int y = 90; y < 130; y++){ // 倒三角
            for(int x = 650 - i; x <= 650 + i; x++){
                int p=4*(1024*y+x); share[p]=0; share[p+1]=255; share[p+2]=0; share[p+3]=0;
            }
            i--;
        }
    }
    DRMshowUp(fb, &drm);
}

// ================== 3. 触摸检测 (核心) ==================
void touch_detect(){
    struct input_event ts;
    // 非阻塞读取
    if(read(touch, &ts, sizeof(ts)) > 0){
        static int x = 0, y = 0;
        if(ts.type == EV_ABS && ts.code == ABS_X) x = ts.value;
        if(ts.type == EV_ABS && ts.code == ABS_Y) y = ts.value;
        
        // 手指松开
        if(ts.type == EV_KEY && ts.code == BTN_TOUCH && ts.value == 0){
            if(y > 300){
                int col = 0;
                if(x > 60 && x < 180) col = 1;
                if(x > 260 && x < 380) col = 2;
                if(x > 460 && x < 580) col = 3;
                
                int need_refresh = 0;
                
                // 1-3楼
                if(y > 350 && y < 460){
                    if(col==1) { floor_flag[1]=1; need_refresh=1; }
                    if(col==2) { floor_flag[2]=1; need_refresh=1; }
                    if(col==3) { floor_flag[3]=1; need_refresh=1; }
                    // 开门键
                    if(x > 800) { 
                        door_state = 1; door_timer = 40; door_show(); 
                    }
                }
                // 4-6楼
                if(y > 480 && y < 590){
                    if(col==1) { floor_flag[4]=1; need_refresh=1; }
                    if(col==2) { floor_flag[5]=1; need_refresh=1; }
                    if(col==3) { floor_flag[6]=1; need_refresh=1; }
                    // 关门键
                    if(x > 800) { door_timer = 0; }
                }
                
                // 如果有楼层被按下，马上刷新按钮状态
                if(need_refresh == 1) button_show();
            }
        }
    }
}

// ================== 4. 主函数 ==================
int main(){
    fb = open("/dev/dri/card0", O_RDWR);
    if(fb == -1) return -1;
    DRMinit(fb); DRMcreateFB(fb, &drm);
    
    touch = open("/dev/input/event2", O_RDWR | O_NONBLOCK);
    if(touch == -1) return -1;

    // 初始化背景: 上黑下白
    char *share = drm.vaddr;
    for(int i=0; i<1024*600; i++) {
        if(i < 1024*300) ((int*)share)[i] = 0; 
        else ((int*)share)[i] = 0xFFFFFFFF;
    }
    DRMshowUp(fb, &drm);

    // 初始画面
    button_show();
    if(current_floor == 1) bmp_show("anniu1.bmp", 450, 80); // 初始显示1楼
    door_show();

    int target = 0;

    
    // 初始化背景等代码...
    door_show();

    int run_dir = 0; // 记录运行方向：0静止，1上，-1下

        // ...
    while(1){
        // 1. 持续检测按键
        touch_detect();

        // 2. 门逻辑
        if(door_state == 1){
            if(door_timer > 0){
                door_timer--;
                usleep(50000); // 0.05秒
            } else {
                door_state = 0;
                door_show(); // 变红
                usleep(500000); 
            }
            continue;
        }

        // 3. 寻找目标
        target = 0;
        




        // 如果当前正在向上(run_dir==1)，优先找头顶上的楼层
        if(run_dir == 1) {
            for(int i = current_floor + 1; i <= 6; i++){
                if(floor_flag[i] == 1) { target = i; break; }
            }
        }
        // 如果当前正在向下(run_dir==-1)，优先找脚下的楼层
        else if(run_dir == -1) {
            for(int i = current_floor - 1; i >= 1; i--){
                if(floor_flag[i] == 1) { target = i; break; }
            }
        }
        
        // 如果顺路没找到目标（或者处于静止状态），那就全局搜索
        if(target == 0) {
            for(int i = 1; i <= 6; i++){
                // 只要有按下的，且不是当前层，就走
                if(floor_flag[i] == 1 && i != current_floor) { target = i; break; }
            }
        }

        // 找到目标后，立刻更新“方向记忆”，供下一次循环判断
        if(target > current_floor) run_dir = 1;      // 准备向上
        else if(target < current_floor) run_dir = -1;// 准备向下
        else run_dir = 0;                            // 没任务，静止
        
    

        // 4. 移动
        if(target != 0){
            int dir = 0;
            if(target > current_floor) dir = 1; else dir = -1;
            
            // 循环40次 = 2秒
            for(int k = 0; k < 40; k++){
                touch_detect(); // 移动时也要检测！
                
                if(dir == 1) arrow_show(1);
                else arrow_show(2);
                
                usleep(50000); 
            }

            // 到了新楼层
            current_floor += dir;
            
            // 修改点：用最简单的 if 语句来决定显示哪张图 (符合你的要求)
            if(current_floor == 1) bmp_effect("anniu1.bmp", 450, 80);
            if(current_floor == 2) bmp_effect("anniu2.bmp", 450, 80);
            if(current_floor == 3) bmp_effect("anniu3.bmp", 450, 80);
            if(current_floor == 4) bmp_effect("anniu4.bmp", 450, 80);
            if(current_floor == 5) bmp_effect("anniu5.bmp", 450, 80);
            if(current_floor == 6) bmp_effect("anniu6.bmp", 450, 80);

            // 到达检测
            if(floor_flag[current_floor] == 1){
                floor_flag[current_floor] = 0;
                button_show();
                
                door_state = 1; door_timer = 40; door_show();
                
                // 擦除箭头
                for(int y=50; y<150; y++) for(int x=600; x<700; x++)
                    { int p=4*(1024*y+x); share[p]=0;share[p+1]=0;share[p+2]=0;share[p+3]=0; }
                DRMshowUp(fb, &drm);
                
                usleep(500000); 
            }
        } else {
            usleep(50000);
        }
    }
    
    close(touch);
    close(fb);
    return 0;
}