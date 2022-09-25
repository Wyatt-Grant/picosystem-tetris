#include <math.h>
#include <algorithm>
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "picosystem.hpp"

using namespace picosystem;

#include "titlescreen.cpp"
#include "notes.cpp"
#include "songs.cpp"
#include "blocks.cpp"

//saving stuff
#define ADDRESS 0x1003F204
#define ERASE_SIZE 4096
#define WRITE_SIZE 4
uint8_t *readSave = (uint8_t *) (ADDRESS);
unsigned char saveBuffer[WRITE_SIZE];

//music stuff
bool muted = false;
int32_t tempo = title_tempo;
int32_t notes = sizeof(title_melody) / sizeof(title_melody[0]) / 2;
int32_t wholenote = (60000 * 4) / tempo;
int32_t divider = 0, noteDuration = 0;

//game stuff
int32_t hiscore;
int32_t gameSpeed = 30;
int32_t blocktimer = 0;

int32_t currentBlock[4][4];
int32_t currentBlockWidth;
int32_t nextBlock[4][4];
int32_t blockX = 4;
int32_t blockY = 0;
int32_t lineClearedCounter = 0;
bool justHardDropped = false;

int32_t lines = 0;
int32_t score = 0;
int32_t level = 0;

std::vector<int32_t> randomBag;

//scene stuff
bool showTitle = true;
bool showGameOver = false;
bool restartSong = false;

int32_t board[20][10] = {
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0}
};

void writeScores() {
   uint32_t ints = save_and_disable_interrupts();
   flash_range_erase(ADDRESS - XIP_BASE, ERASE_SIZE);
   flash_range_program(ADDRESS - XIP_BASE, saveBuffer, WRITE_SIZE);
   restore_interrupts(ints);
}

void saveMyData() {
    saveBuffer[0] = (hiscore >> 24) & 0xFF;
    saveBuffer[1] = (hiscore >> 16) & 0xFF;
    saveBuffer[2] = (hiscore >> 8) & 0xFF;
    saveBuffer[3] = hiscore & 0xFF;
    writeScores();
}

void readBackMyData() {
    hiscore = (readSave[0] << 24 | readSave[1] << 16 | readSave[2] << 8 | readSave[3]);
}

void handleMusic() {
    while(1) {
        for (int32_t thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
            if (showGameOver) {
                divider = gameover_melody[thisNote + 1];
            } else if (showTitle) {
                divider = title_melody[thisNote + 1];
            } else {
                divider = game_melody[thisNote + 1];
            }

            if (divider > 0) {
                noteDuration = (wholenote) / divider;
            } else if (divider < 0) {
                noteDuration = (wholenote) / abs(divider);
                noteDuration *= 1.5;
            }

            if (showGameOver) {
                play(piano, gameover_melody[thisNote], noteDuration * 0.9, muted ? 0 : 100);
            } else if (showTitle) {
                play(piano, title_melody[thisNote], noteDuration * 0.9, muted ? 0 : 100);
            } else {
                play(piano, game_melody[thisNote], noteDuration * 0.9, muted ? 0 : 100);
            }
            sleep(noteDuration);

            if (restartSong) {
                restartSong = false;
                break;
            }
        }
    }
}

void setColor(int32_t cb[][4], int32_t c) {
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (cb[y][x] != 0) {
                cb[y][x] = c;
            }
        }
    }
}

void checkAndFillRandomBag() {
    if (randomBag.empty()) {
        for (int32_t i = 1; i <= 7; i++) {
            randomBag.push_back(i);
        }
        std::random_shuffle(randomBag.begin(), randomBag.end());
    }
}

void setBlockToRandom(int32_t block[][4]) {
    checkAndFillRandomBag();
    switch(randomBag.front()) {
        case 1:
            std::copy(&block1[0][0], &block1[0][0]+4*4,&block[0][0]);
            setColor(block, 1);
            break;
        case 2:
            std::copy(&block2[0][0], &block2[0][0]+4*4,&block[0][0]);
            setColor(block, 2);
            break;
        case 3:
            std::copy(&block3[0][0], &block3[0][0]+4*4,&block[0][0]);
            setColor(block, 3);
            break;
        case 4:
            std::copy(&block4[0][0], &block4[0][0]+4*4,&block[0][0]);
            setColor(block, 4);
            break;
        case 5:
            std::copy(&block5[0][0], &block5[0][0]+4*4,&block[0][0]);
            setColor(block, 5);
            break;
        case 6:
            std::copy(&block6[0][0], &block6[0][0]+4*4,&block[0][0]);
            setColor(block, 6);
            break;
        case 7:
            std::copy(&block7[0][0], &block7[0][0]+4*4,&block[0][0]);
            setColor(block, 7);
            break;
    }
    randomBag.erase(randomBag.begin());
}

void resetGame() {
    if (hiscore < score) {
        hiscore = score;       
        saveMyData();
        readBackMyData();
    }

    lines = 0;
    level = 0;
    blockX = 4;
    blockY = 0;
    gameSpeed = 30;
    blocktimer = 0;
    for (int32_t x = 0; x < 10; x++) {
        for (int32_t y = 0; y < 20; y++) {
            board[y][x] = 0;
        }
    }
    setBlockToRandom(nextBlock);
    setBlockToRandom(currentBlock);

    restartSong = true;
    tempo = gameover_tempo;
    notes = sizeof(gameover_melody) / sizeof(gameover_melody[0]) / 2;
    wholenote = (60000 * 4) / gameover_tempo;
    divider = 0;
    noteDuration = 0;
}

void shiftNonZeroRight(int32_t cb[][4]) {
    bool shouldShift = true;
    for (int32_t y = 0; y < 4; y++) {
        if (cb[y][0] != 0) {
            shouldShift = false;
        }
    }

    for(int32_t i = 0; i < 4; i++){
        if (shouldShift) {
            for (int32_t x = 0; x < 3; x++) {
                cb[i][x] = cb[i][x+1];
            }
            cb[i][3] = 0;
        }
    }
}

void shiftNonZeroUp(int32_t cb[][4]) {
    for(int32_t y = 0; y < 4; y++){
        int32_t empty[4] = {0,0,0,0};
        bool isAllZero = true;

        for(int32_t x = 0; x < 4; x++){
            if (cb[0][x] != 0) {
                isAllZero = false;
            }
        }

        if (isAllZero) {
            for (int32_t i = 0; i < 3; i++) {
                std::copy(std::begin(cb[i+1]), std::end(cb[i+1]), std::begin(cb[i]));
            }
            
            std::copy(std::begin(empty), std::end(empty), std::begin(cb[3]));
        } else {
            return;
        }
    }
}

void rotateCW(int32_t cb[][4]) {
    int32_t n = 4;
    for (int32_t i=0;i<n/2;i++) { 
        for (int32_t j=i;j<n-i-1;j++) {
            int32_t temp=cb[i][j]; 
            cb[i][j]=cb[n-1-j][i]; 
            cb[n-1-j][i]=cb[n-1-i][n-1-j]; 
            cb[n-1-i][n-1-j]=cb[j][n-1-i]; 
            cb[j][n-1-i]=temp; 
        } 
    }
}

void rotateCCW(int32_t cb[][4]) {
    int32_t n = 4;
    for(int32_t i=0;i<n/2;i++) {
        for(int32_t j=i;j<n-i-1;j++) {
            int32_t temp=cb[i][j];
            cb[i][j]=cb[j][n-i-1];
            cb[j][n-i-1]=cb[n-i-1][n-j-1];
            cb[n-i-1][n-j-1]=cb[n-j-1][i];
            cb[n-j-1][i]=temp;
        }
    }
}

void calcBlockWidth(){
    currentBlockWidth = 0;
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (currentBlock[y][x] != 0 && x > currentBlockWidth) {
                currentBlockWidth = x;
            }
        }
    }
}

bool willHitBottom() {
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (currentBlock[y][x] != 0 && (y + blockY) > 18) {
                return true;
            }
        }
    }

    return false;
}

bool willHitBoardBricks(int32_t bx, int32_t by) {
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (board[y + by][x + bx] != 0 && currentBlock[y][x] != 0) {
                return true;
            }
        }
    }

    return false;
}

void copyCurrentBlockToBoard() {
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (currentBlock[y][x] != 0) {
                board[y + blockY][x + blockX] = currentBlock[y][x];
            }
        }
    }
}

bool clearLines() {
    for (int32_t y = 0; y < 20; y++) {
        bool shouldClearRow = true;
        for (int32_t x = 0; x < 10; x++) {
            if (board[y][x] == 0) {
                shouldClearRow = false;
            }
        }

        if (shouldClearRow) {
            lines += 1;
            for (int32_t x = 0; x < 10; x++) {
                board[y][x] = 0;
            }

            for(int32_t z = y; z > 0; z--){
                std::copy(std::begin(board[z-1]), std::end(board[z-1]), std::begin(board[z]));
            }
            return true;
        }
    }

    return false;
}

void hardDrop() {
    justHardDropped = true;
    while(!willHitBoardBricks(blockX, blockY+1) && !willHitBottom()) {
        blockY += 1;
    }
}

void drawBrick(uint32_t posX, uint32_t posY, uint32_t color) {
    switch(color) {
        case 1:
            pen(15,0,0);
            break;
        case 2:
            pen(15,0,15);
            break;
        case 3:
            pen(12,12,12);
            break;
        case 4:
            pen(0,15,0);
            break;
        case 5:
            pen(0,15,15);
            break;
        case 6:
            pen(0,0,15);
            break;
        case 7:
            pen(15,15,0);
            break;
    }
    frect(posX, posY, 6, 6);

    switch(color) {
        case 1:
            pen(15,9,9);
            break;
        case 2:
            pen(15,9,15);
            break;
        case 3:
            pen(15,15,15);
            break;
        case 4:
            pen(9,15,9);
            break;
        case 5:
            pen(9,15,15);
            break;
        case 6:
            pen(9,9,15);
            break;
        case 7:
            pen(15,15,9);
            break;
    }
    hline(posX, posY, 6);
    vline(posX, posY, 6);

    switch(color) {
        case 1:
            pen(9,0,0);
            break;
        case 2:
            pen(9,0,9);
            break;
        case 3:
            pen(9,9,9);
            break;
        case 4:
            pen(0,9,0);
            break;
        case 5:
            pen(0,9,9);
            break;
        case 6:
            pen(0,0,9);
            break;
        case 7:
            pen(9,9,0);
            break;
    }
    hline(posX, posY + 5, 6);
    vline(posX + 5, posY, 6);
}

void drawBoard() {
    for (int32_t x = 0; x < 10; x++) {
        for (int32_t y = 0; y < 20; y++) {
            if (board[y][x] != 0) {
                drawBrick(x * 6, y * 6, board[y][x]);
            }
        }
    }

    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (currentBlock[y][x] != 0) {
                drawBrick((x + blockX) * 6, (y + blockY) * 6, currentBlock[y][x]);
            }
        }
    }
}

void noCornerBox(int32_t x, int32_t y, int32_t w, int32_t h) {
    hline(x+1, y,   w-2);
    hline(x+1, y+h-1, w-2);
    vline(x, y+1,   h-2);
    vline(x+w-1, y+1, h-2);
}

void drawWindow(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    pen(15,15,15);
    noCornerBox(x,y,w,h);
    frect(x+1,y+1,w-2,h-2);
    pen(3,3,3);
    noCornerBox(x+1,y+1,w-2,h-2);
}

void init() { 
    readBackMyData();
    if(hiscore < 0){
        hiscore = 0;       
        saveMyData();
    }

    multicore_launch_core1(handleMusic);
}

void update(uint32_t tick) {
    if (pressed(X)) {
        muted = !muted;
    }

    if (showGameOver) {
        if (pressed(A)) {
            restartSong = true;
            showGameOver = false;
            tempo = title_tempo;
            notes = sizeof(title_melody) / sizeof(title_melody[0]) / 2;
            wholenote = (60000 * 4) / title_tempo;
            divider = 0;
            noteDuration = 0;
        }
        return;
    }

    if (showTitle) {
        if (pressed(A)) {
            restartSong = true;
            srand(time());
            randomBag.clear();
            showTitle = false;
            setBlockToRandom(nextBlock);
            setBlockToRandom(currentBlock);
            score = 0;

            tempo = game_tempo;
            notes = sizeof(game_melody) / sizeof(game_melody[0]) / 2;
            wholenote = (60000 * 4) / game_tempo;
            divider = 0;
            noteDuration = 0;
        }
        return;
    }

    for (int32_t i = 0; i < 10; i++) {
        if (board[0][i] != 0) {
            showGameOver = true;
            resetGame();
        }
    }

    //move block left/right
    if (pressed(LEFT)) {
        if (!willHitBoardBricks(blockX-1, blockY)) {
            blockX -= 1;
            srand(time());
        }
    }
    if (pressed(RIGHT)) {
        if (!willHitBoardBricks(blockX+1, blockY)) {
            blockX += 1;
            srand(time());
        }
    }

    // move block down
    blocktimer += 1;
    if (blocktimer == (gameSpeed - level) || button(DOWN)) {
        if (willHitBoardBricks(blockX, blockY + 1) || willHitBottom()) {
            if (justHardDropped) {
                score += (currentBlockWidth + 1) * 2;
            } else {
                score += (currentBlockWidth + 1);
            }
            copyCurrentBlockToBoard();
            blockX = 4;
            blockY = 0;
            blocktimer = 0;
            std::copy(&nextBlock[0][0], &nextBlock[0][0]+4*4,&currentBlock[0][0]);
            setBlockToRandom(nextBlock);
        } else {
            blockY += 1;
        }

        blocktimer = 0;
    }

    // rotate block
    if (pressed(A)) {
        rotateCW(currentBlock);
        if (willHitBoardBricks(blockX, blockY)) {
            rotateCCW(currentBlock);
        }
        srand(time());
    }

    if (pressed(Y)) {
        rotateCCW(currentBlock);
        if (willHitBoardBricks(blockX, blockY)) {
            rotateCW(currentBlock);
        }
        srand(time());
    }

    if (pressed(B)) {
        hardDrop();
    }

    shiftNonZeroRight(currentBlock);
    shiftNonZeroUp(currentBlock);
    calcBlockWidth();

    if (blockX < 0) {
        blockX = 0;
    }
    if (blockX > 9 - currentBlockWidth) {
        blockX = 9 - currentBlockWidth;
    }

    if (clearLines()) {
        lineClearedCounter += 1;
    } else {
        switch (lineClearedCounter) {
            case 1:
                score += (40 * (level + 1));	
                break;
            case 2:
                score += (100 * (level + 1));
                break;
            case 3:
                score += (300 * (level + 1));
                break;
            case 4:
                score += (1200 * (level + 1));
                break;
        }

        lineClearedCounter = 0;
    }

    level = lines/10;
}

void draw(uint32_t tick) {
    pen(0,0,0);
    clear();

    if (showGameOver) {
        pen(15,15,15);
        text("Game Over :(", 30, 40);
        std::string msg = "score: ";
        text(msg.append(str(score)), 30, 56);
        showTitle = true;

        text("press A to go title", 20, 112);
        return;
    }

    if (showTitle) {
        spritesheet(title_sprite_sheet);
        for (int32_t x = 0; x < 15; x++) {
            for (int32_t y = 0; y < 15; y++) {
                sprite((y*15)+x,(x*8),(y*8));
            }
        }

        text("press A to start", 24, 102);
        text("press X to mute", 24, 111);


        pen(0,0,0);
        text("hi-score:", 8, 54);
        text(str(hiscore), 8, 62);
        return;
    }


    pen(15,15,15);
    frect(60, 12, 60, 20);

    pen(5,5,5);
    frect(60, 13, 60, 6);
    hline(60, 20, 60);
    hline(60, 30, 60);

    drawWindow(74,4,36,14);
    
    pen(0,0,0);
    text("score", 78, 7);
    text(str(score), 68, 22);


    drawWindow(74,36,36,23);

    pen(0,0,0);
    text("level", 80, 40);
    text(str(level), 80, 48);

    drawWindow(74,62,36,23);

    pen(0,0,0);
    text("lines", 80, 66);
    text(str(lines), 80, 74);

    pen(5,5,5);
    vline(60,0,120);


    drawWindow(77,89,30,30);

    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            if (nextBlock[y][x] != 0) {
                drawBrick((x * 6) + 80, (y * 6) + 92, nextBlock[y][x]);
            }
        }
    }

    drawBoard();
}