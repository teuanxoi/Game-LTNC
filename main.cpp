#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>

const int SCREEN_WIDTH = 350;
const int SCREEN_HEIGHT = 485;
const int GROUND_HEIGHT = 50;
const int GRAVITY = 1;
const int JUMP_STRENGTH = -9;
const int PIPE_WIDTH = 70;
const int PIPE_GAP = 160;
const int PIPE_SPEED = 3;
const int GROUND_SPEED = 3;

std::vector<SDL_Texture*> numberTextures(10);
bool loadNumberTextures(SDL_Renderer* renderer) {
    for (int i = 0; i < 10; i++) {
        std::string path = "res/number/large/" + std::to_string(i) + ".png";
        numberTextures[i] = IMG_LoadTexture(renderer, path.c_str());
        if (!numberTextures[i]) {
            std::cerr << "Failed to load number texture: " << path << " - " << SDL_GetError() << std::endl;
            return false;
        }
    }
    return true;
}
void renderScore(SDL_Renderer* renderer, int score, int x, int y, int digitWidth = 24, int digitHeight = 36) {
    std::string scoreStr = std::to_string(score);
    for (size_t i = 0; i < scoreStr.length(); i++) {
        int digit = scoreStr[i] - '0';
        if (digit >= 0 && digit <= 9) {
            SDL_Rect destRect = {
                x + static_cast<int>(i) * (digitWidth + 2),
                y,
                digitWidth,
                digitHeight
            };
            SDL_RenderCopy(renderer, numberTextures[digit], NULL, &destRect);
        }
    }
}void cleanupNumberTextures() {
    for (auto texture : numberTextures) {
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }
}
struct Pipe {
    int x, height;
    bool passed;
    int yVelocity;
    bool canMove;
    Pipe(int startX, int h, bool canMoveFrag) : x(startX), height(h), passed(false), canMove(canMoveFrag) {
        yVelocity = ((rand()%3)-1);
        if (yVelocity==0) yVelocity=1;
    }
    void update(){
        if(canMove){
            height+=yVelocity;
            if(height<50||height>SCREEN_HEIGHT/2){
                yVelocity=-yVelocity;
            }
        }
    }
};
class Player {
public:
    SDL_Texture* texture;
    int x, y, velocity;

    Player(SDL_Texture* tex, int startX, int startY) {
        texture = tex;
        x = startX;
        y = startY;
        velocity = 0;
    }
    void update(bool gameStarted, bool &gameOver,bool soundEnable, Mix_Chunk* deadthSound, bool &soundPlayed) {
        if (gameStarted && !gameOver ) {
            velocity += GRAVITY;
            y += velocity;
            if (y < 0) {
                y=0;
                velocity=0;
            }
            if (y + 32 > SCREEN_HEIGHT - GROUND_HEIGHT) {
                y = SCREEN_HEIGHT - GROUND_HEIGHT - 32;
                velocity = 0;
                    gameOver=true;
                    if(soundEnable&& !soundPlayed){
                        Mix_PlayChannel(-1, deadthSound, 0);
                        soundPlayed = true;
                    }

            }
        }
    }
    void jump() {
        if (y + 32 < SCREEN_HEIGHT - GROUND_HEIGHT) {
            velocity = JUMP_STRENGTH;
        }
    }
};
int main(int argc, char* args[]) {
    if (SDL_Init(SDL_INIT_VIDEO) > 0) {
        std::cout << "Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cout << "Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    if (Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return -1;
    }


    SDL_Window* window = SDL_CreateWindow("Flying Dog", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* background = IMG_LoadTexture(renderer, "res/image/background.png");
    SDL_Texture* groundTexture = IMG_LoadTexture(renderer, "res/image/land.png");
    SDL_Texture* dogTexture = IMG_LoadTexture(renderer, "res/image/shiba.png");
    SDL_Texture* pipeTexture = IMG_LoadTexture(renderer, "res/image/pipe.png");
    SDL_Texture* getReadyTexture = IMG_LoadTexture(renderer, "res/image/message.png");
    SDL_Texture* gameOverTexture = IMG_LoadTexture(renderer, "res/image/gameOver.png");
    SDL_Texture* pauseTexture = IMG_LoadTexture(renderer, "res/image/pause.png");
    SDL_Texture* pauseTabTexture = IMG_LoadTexture(renderer, "res/image/pauseTab.png");
    SDL_Texture* resumeTexture = IMG_LoadTexture(renderer, "res/image/resume.png");
    SDL_Texture* soundIcon = IMG_LoadTexture(renderer, "res/image/sound.png");
    SDL_Texture* replayIcon = IMG_LoadTexture(renderer, "res/image/replay.png");
    Mix_Chunk* jumpSound = Mix_LoadWAV("res/sound/sfx_breath.wav");
    Mix_Chunk* deathSound = Mix_LoadWAV("res/sound/sfx_bonk.wav");

    if (!loadNumberTextures(renderer)) {
        std::cout << "Failed to load number textures!" << std::endl;
        return -1;
    }
    Player player(dogTexture, 50, SCREEN_HEIGHT / 2);
    std::vector<Pipe> pipes;
    int pipespeed = PIPE_SPEED;
    int groundspeed = GROUND_SPEED;
    int groundX = 0;
    int score = 0;
    int bestScore = 0;
    srand(time(0));

    SDL_Rect soundClip[2] = {{0, 0, 32, 24},{0, 24, 32, 24}};

    std::ifstream inFile("res/data/bestScore.txt");
    if (inFile){
        inFile >> bestScore;
    }
    inFile.close();

    bool gameRunning = true;
    bool gameStarted = false;
    bool gameOver = false;
    bool isPaused = false;
    bool soundEnable = true;
    bool soundPlayed=false;
    SDL_Event event;

    while (gameRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                gameRunning = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (!gameStarted) {
                        gameStarted = true;
                    } else if (!isPaused && !gameOver) {
                        player.jump();
                        if(soundEnable){
                            Mix_PlayChannel(-1, jumpSound, 0);
                        }
                    }
                } else if (event.key.keysym.sym == SDLK_p || event.key.keysym.sym == SDLK_ESCAPE) {
                    isPaused = !isPaused;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN ) {
                    if(isPaused){
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;
                        if (mouseX >= SCREEN_WIDTH - 60 && mouseX <= SCREEN_WIDTH - 10 &&
                            mouseY >= 10 && mouseY <= 60) {
                            isPaused = !isPaused;
                            }
                        if (mouseX >= (SCREEN_WIDTH-250) /2 + 50 && mouseX <= (SCREEN_WIDTH-250)/2+50 + 32 &&
                            mouseY >= (SCREEN_HEIGHT-128) /2 + 40 && mouseY <= (SCREEN_HEIGHT-128)/2+40 + 24 ){
                            soundEnable=!soundEnable;
                            Mix_Volume(-1, soundEnable ? MIX_MAX_VOLUME : 0);
                        }
                    }
                    if(gameOver){
                        int mouseX = event.button.x;
                        int mouseY = event.button.y;
                        if (mouseX >= (SCREEN_WIDTH-100)/2 && mouseX <= (SCREEN_WIDTH-100)/2 + 100 &&
                            mouseY >= (SCREEN_HEIGHT-56)/2+100 && mouseY <= (SCREEN_HEIGHT-56)/2+100 + 56) {
                            gameOver = false;
                            gameStarted = false;
                            pipes.clear();
                            player.y = SCREEN_HEIGHT / 2;
                            player.velocity = 0;
                            score = 0;
                            soundPlayed=false;
                        }

                    }
                }
            }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, background, NULL, NULL);

        if (!gameStarted) {
            SDL_Rect getReadyRect = { (SCREEN_WIDTH - 225) / 2, (SCREEN_HEIGHT - 204) / 2, 225, 204 };
            SDL_RenderCopy(renderer, getReadyTexture, NULL, &getReadyRect);
        }

        if (!isPaused) {
            player.update(gameStarted, gameOver, soundEnable, deathSound, soundPlayed);
        }

        if (gameStarted && !gameOver && !isPaused) {
            if (pipes.empty() || pipes.back().x < SCREEN_WIDTH - 210) {
                int randomHeight = rand() % (SCREEN_HEIGHT / 2) + 50;
                bool moveAble = (score >= 20);
                pipes.emplace_back(SCREEN_WIDTH, randomHeight, moveAble);
            }

            for (auto& pipe : pipes) {
                pipe.x -= pipespeed;
                pipe.update();
                if(score>=20){
                    pipe.canMove = true;
                }
                if (!pipe.passed && player.x > pipe.x + PIPE_WIDTH) {
                    pipe.passed = true;
                    score++;
                    if (bestScore < score) {
                        bestScore = score;
                        std::ofstream outFile("res/data/bestScore.txt");
                        if (outFile){
                            outFile << bestScore;
                        }
                        outFile.close();
                    }
                }
                if (player.x + 32 > pipe.x && player.x < pipe.x + PIPE_WIDTH) {
                    if (player.y < pipe.height || player.y + 32 > pipe.height + PIPE_GAP) {
                            if(!gameOver){
                                gameOver = true;
                                player.velocity=0;
                                if(soundEnable && !soundPlayed){
                                    Mix_PlayChannel(-1, deathSound, 0);
                                }
                            }
                    }
                }
            }
            if (!pipes.empty() && pipes.front().x < -PIPE_WIDTH) {
                pipes.erase(pipes.begin());
            }
        }
        if(score%5==0){
            pipespeed = PIPE_SPEED + score/5;
            groundspeed = GROUND_SPEED + score/5;
        }
        if (!gameOver && !isPaused) {
            groundX -= groundspeed;
            if (groundX <= -SCREEN_WIDTH) groundX = 0;
        }

        for (const auto& pipe : pipes) {
            SDL_Rect topPipe = { pipe.x, pipe.height - 320, PIPE_WIDTH, 320 };
            SDL_Rect bottomPipe = { pipe.x, pipe.height + PIPE_GAP, PIPE_WIDTH, 320 };
            SDL_RenderCopy(renderer, pipeTexture, NULL, &topPipe);
            SDL_RenderCopyEx(renderer, pipeTexture, NULL, &bottomPipe, 0, NULL, SDL_FLIP_VERTICAL);
        }

        SDL_Rect playerRect = { player.x, player.y, 50, 35 };
        SDL_RenderCopy(renderer, player.texture, NULL, &playerRect);

        SDL_Rect groundRect = { groundX, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT };
        SDL_RenderCopy(renderer, groundTexture, NULL, &groundRect);
        SDL_Rect groundRect2 = { groundX + SCREEN_WIDTH, SCREEN_HEIGHT - GROUND_HEIGHT, SCREEN_WIDTH, GROUND_HEIGHT };
        SDL_RenderCopy(renderer, groundTexture, NULL, &groundRect2);

        if (gameStarted && !gameOver && !isPaused) {
            renderScore(renderer, score, SCREEN_WIDTH / 2 - std::to_string(score).length() * 13, 50);
        }
        if (gameOver) {
            SDL_Rect gameOverRect = { (SCREEN_WIDTH - 250) / 2, (SCREEN_HEIGHT - 209) / 2, 250, 209 };
            SDL_RenderCopy(renderer, gameOverTexture, NULL, &gameOverRect);

            SDL_Rect replayRect = { (SCREEN_WIDTH-100)/2, (SCREEN_HEIGHT-56)/2+100, 100, 56 };
            SDL_RenderCopy(renderer, replayIcon, NULL, &replayRect);

            renderScore(renderer, score, SCREEN_WIDTH / 2 - std::to_string(score).length() * 13 + 68 , SCREEN_HEIGHT / 2 + 15, 16, 21);
            renderScore(renderer, bestScore, SCREEN_WIDTH/2 - std::to_string(bestScore).length()*13 - 55, SCREEN_HEIGHT/2 + 15, 16, 21);
        }

        if (gameStarted && !gameOver) {
            SDL_Rect iconRect = { SCREEN_WIDTH - 45, 15, 30, 30 };
            if (isPaused) {
                SDL_RenderCopy(renderer, resumeTexture, NULL, &iconRect);
            } else {
                SDL_RenderCopy(renderer, pauseTexture, NULL, &iconRect);
            }
        }

        if (isPaused) {
            SDL_Rect pauseTabRect = { (SCREEN_WIDTH - 250) / 2, (SCREEN_HEIGHT - 128) / 2, 250, 128 };
            SDL_RenderCopy(renderer, pauseTabTexture, NULL, &pauseTabRect);

            SDL_Rect soundIconRect = { (SCREEN_WIDTH-250) / 2 + 50, (SCREEN_HEIGHT - 128) / 2 + 40, 32, 24};
            SDL_RenderCopy(renderer, soundIcon, &soundClip[soundEnable ? 0 : 1] , &soundIconRect);

            renderScore(renderer, score, (SCREEN_WIDTH / 2 - std::to_string(score).length() * 13 + 80), SCREEN_HEIGHT / 2 - 30, 20, 30);
            renderScore (renderer, bestScore, (SCREEN_WIDTH/2 - std::to_string(bestScore).length() * 13 + 80), SCREEN_HEIGHT/2 + 20, 20, 30);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(25);
    }

    SDL_Quit();

    return 0;
}
