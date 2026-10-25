#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <random>

using std::min;
using std::max;

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Math Utilities ---
struct Vec3 {
    float x, y, z;
    Vec3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float l = length();
        return l > 0.0001f ? Vec3(x / l, y / l, z / l) : Vec3(0, 0, 0);
    }
    float dist(const Vec3& o) const { return (*this - o).length(); }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
};

// --- In-Memory Procedural Audio (No external assets) ---
class AudioEngine {
public:
    std::vector<BYTE> wavShotgun, wavPlasma, wavRocket, wavExplosion, wavPickup, wavHit, wavChainsaw;

    void init() {
        // Super Shotgun blast (deep punch + loud white noise)
        wavShotgun = makeWav(22050, 0.25f, [](float t) {
            float noise = ((float)(rand() % 2000) / 1000.0f - 1.0f);
            float low = std::sin(t * 110.0f * 2.0f * (float)M_PI);
            return (low * 0.5f + noise * 0.5f) * (1.0f - t / 0.25f);
        });

        // Plasma rifle (high-frequency electric beep)
        wavPlasma = makeWav(22050, 0.10f, [](float t) {
            float freq = 850.0f - t * 3000.0f;
            return std::sin(t * freq * 2.0f * (float)M_PI) * (1.0f - t / 0.10f);
        });

        // Rocket Launch (rising whoosh)
        wavRocket = makeWav(22050, 0.18f, [](float t) {
            float freq = 200.0f + t * 600.0f;
            float noise = ((float)(rand() % 2000) / 1000.0f - 1.0f) * 0.3f;
            return (std::sin(t * freq * 2.0f * (float)M_PI) + noise) * (1.0f - t / 0.18f);
        });

        // Rocket Explosion (heavy sub-bass + noise)
        wavExplosion = makeWav(22050, 0.45f, [](float t) {
            float noise = ((float)(rand() % 2000) / 1000.0f - 1.0f);
            float low = std::sin(t * 70.0f * 2.0f * (float)M_PI);
            return (low * 0.7f + noise * 0.3f) * (1.0f - t / 0.45f);
        });

        // Item Pickup (cheerful high arpeggio)
        wavPickup = makeWav(22050, 0.15f, [](float t) {
            float freq = (t < 0.07f) ? 587.33f : 880.0f;
            return std::sin(t * freq * 2.0f * (float)M_PI) * (1.0f - t / 0.15f) * 0.6f;
        });

        // Monster Hit / Pain
        wavHit = makeWav(22050, 0.12f, [](float t) {
            float freq = 140.0f - t * 400.0f;
            return std::sin(t * freq * 2.0f * (float)M_PI) * (1.0f - t / 0.12f);
        });

        // Chainsaw rev
        wavChainsaw = makeWav(22050, 0.14f, [](float t) {
            float saw = (std::sin(t * 220.0f * 2.0f * (float)M_PI) > 0.0f ? 1.0f : -1.0f);
            return saw * 0.4f * (1.0f - t / 0.14f);
        });
    }

    void playShotgun() { PlaySoundA((LPCSTR)wavShotgun.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playPlasma() { PlaySoundA((LPCSTR)wavPlasma.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playRocket() { PlaySoundA((LPCSTR)wavRocket.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playExplosion() { PlaySoundA((LPCSTR)wavExplosion.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playPickup() { PlaySoundA((LPCSTR)wavPickup.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playHit() { PlaySoundA((LPCSTR)wavHit.data(), NULL, SND_MEMORY | SND_ASYNC); }
    void playChainsaw() { PlaySoundA((LPCSTR)wavChainsaw.data(), NULL, SND_MEMORY | SND_ASYNC); }

private:
    template<typename F>
    std::vector<BYTE> makeWav(int sampleRate, float duration, F gen) {
        int numSamples = (int)(sampleRate * duration);
        int dataSize = numSamples * 2;
        int fileSize = 44 + dataSize;
        std::vector<BYTE> buf(fileSize);

        memcpy(&buf[0], "RIFF", 4);
        int s = fileSize - 8; memcpy(&buf[4], &s, 4);
        memcpy(&buf[8], "WAVEfmt ", 8);
        int sub1 = 16; memcpy(&buf[16], &sub1, 4);
        short fmt = 1; memcpy(&buf[20], &fmt, 2);
        short channels = 1; memcpy(&buf[22], &channels, 2);
        memcpy(&buf[24], &sampleRate, 4);
        int byteRate = sampleRate * 2; memcpy(&buf[28], &byteRate, 4);
        short blockAlign = 2; memcpy(&buf[32], &blockAlign, 2);
        short bits = 16; memcpy(&buf[34], &bits, 2);
        memcpy(&buf[36], "data", 4);
        memcpy(&buf[40], &dataSize, 4);

        short* samples = (short*)(&buf[44]);
        for (int i = 0; i < numSamples; i++) {
            float t = (float)i / (float)sampleRate;
            float val = gen(t);
            val = max(-1.0f, min(1.0f, val));
            samples[i] = (short)(val * 30000.0f);
        }
        return buf;
    }
};

// --- Weapons ---
enum WeaponType {
    WEAPON_SHOTGUN = 0,
    WEAPON_PLASMA = 1,
    WEAPON_ROCKET = 2,
    WEAPON_CHAINSAW = 3,
    WEAPON_COUNT = 4
};

// --- Projectiles (Rockets, Plasma bolts, Demon fireballs) ---
enum ProjectileType { PROJ_ROCKET, PROJ_PLASMA, PROJ_ENEMY_FIREBALL, PROJ_ENEMY_PLASMA };

struct Projectile {
    ProjectileType type;
    Vec3 pos, vel;
    float radius;
    float damage;
    float life;
    bool isEnemy;
};

// --- Particles & Gibs (Gore chunks) ---
struct Particle {
    Vec3 pos, vel, color;
    float size;
    float life, maxLife;
    bool isGib;
};

// --- Pickups ---
enum PickupType { PICKUP_HEALTH, PICKUP_ARMOR, PICKUP_AMMO_SG, PICKUP_AMMO_PLASMA, PICKUP_AMMO_ROCKET, PICKUP_QUAD };

struct Pickup {
    PickupType type;
    Vec3 pos;
    bool active;
    float respawnTimer;
};

// --- Enemies ---
enum EnemyType { ENEMY_IMP, ENEMY_HELLHOUND, ENEMY_BARON_BOSS };

struct Enemy {
    EnemyType type;
    Vec3 pos, vel;
    float yaw;
    float hp, maxHp;
    float atk;
    float speed;
    float attackTimer;
    float hitFlash;
    bool isDead;
};

// --- Main Engine ---
class DreadfallEngine {
public:
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
    int screenW = 1280, screenH = 720;
    bool keys[256] = { false };
    bool mouseCaptured = true;

    // First-Person Camera
    float camYaw = 0.0f;
    float camPitch = 0.0f;
    float headBob = 0.0f;
    float strafeTilt = 0.0f;
    float screenDamageFlash = 0.0f;
    float screenShake = 0.0f;

    // Player Stats
    Vec3 playerPos = { 0.0f, 0.0f, 0.0f };
    Vec3 playerVel = { 0.0f, 0.0f, 0.0f };
    float playerHp = 100.0f, playerMaxHp = 100.0f;
    float playerArmor = 50.0f, playerMaxArmor = 100.0f;
    bool isGrounded = true;

    // Weapons & Ammo
    WeaponType currentWeapon = WEAPON_SHOTGUN;
    float fireCooldown = 0.0f;
    float weaponRecoil = 0.0f;
    int ammoShotgun = 32;
    int ammoPlasma = 120;
    int ammoRockets = 15;

    // Powerups
    float quadDamageTimer = 0.0f;

    // World & Combat
    int currentWave = 1;
    int kills = 0;
    int score = 0;
    float globalTime = 0.0f;
    bool gameOver = false;
    bool victory = false;

    std::vector<Projectile> projectiles;
    std::vector<Particle> particles;
    std::vector<Pickup> pickups;
    std::vector<Enemy> enemies;

    AudioEngine audio;

    void init(HWND window) {
        hWnd = window;
        hDC = GetDC(hWnd);

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;

        int format = ChoosePixelFormat(hDC, &pfd);
        SetPixelFormat(hDC, format, &pfd);
        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);

        RECT r;
        GetClientRect(hWnd, &r);
        screenW = max(1, (int)(r.right - r.left));
        screenH = max(1, (int)(r.bottom - r.top));

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        glShadeModel(GL_SMOOTH);

        // Dark Atmospheric Lighting
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        float lightDiff[] = { 1.0f, 0.9f, 0.8f, 1.0f };
        float lightAmb[] = { 0.45f, 0.42f, 0.48f, 1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);

        audio.init();
        spawnArenaPickups();
        spawnWave(1);
        captureMouse(true);
    }

    void captureMouse(bool capture) {
        mouseCaptured = capture;
        ShowCursor(!capture);
        if (capture) {
            RECT rect;
            GetClientRect(hWnd, &rect);
            POINT pt = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
            ClientToScreen(hWnd, &pt);
            SetCursorPos(pt.x, pt.y);
        }
    }

    void spawnArenaPickups() {
        pickups.clear();
        // 4 Corners: Medkits and Armor
        pickups.push_back({ PICKUP_HEALTH, { 18.0f, 0.5f, 18.0f }, true, 0.0f });
        pickups.push_back({ PICKUP_HEALTH, { -18.0f, 0.5f, -18.0f }, true, 0.0f });
        pickups.push_back({ PICKUP_ARMOR, { -18.0f, 0.5f, 18.0f }, true, 0.0f });
        pickups.push_back({ PICKUP_ARMOR, { 18.0f, 0.5f, -18.0f }, true, 0.0f });

        // Ammo Pickups
        pickups.push_back({ PICKUP_AMMO_SG, { 0.0f, 0.5f, 14.0f }, true, 0.0f });
        pickups.push_back({ PICKUP_AMMO_PLASMA, { 14.0f, 0.5f, 0.0f }, true, 0.0f });
        pickups.push_back({ PICKUP_AMMO_ROCKET, { -14.0f, 0.5f, 0.0f }, true, 0.0f });

        // Center Arena: QUAD DAMAGE!
        pickups.push_back({ PICKUP_QUAD, { 0.0f, 0.8f, 0.0f }, true, 0.0f });
    }

    void spawnWave(int wave) {
        currentWave = wave;
        enemies.clear();

        if (wave == 1) {
            // Wave 1: 6 Imps
            for (int i = 0; i < 6; i++) {
                float a = (float)i * 2.0f * (float)M_PI / 6.0f;
                enemies.push_back({ ENEMY_IMP, { std::cos(a) * 16.0f, 0.0f, std::sin(a) * 16.0f }, {0,0,0}, 0.0f, 120.0f, 120.0f, 15.0f, 4.0f, 1.5f, 0.0f, false });
            }
        } else if (wave == 2) {
            // Wave 2: 6 Imps + 4 Hellhounds
            for (int i = 0; i < 6; i++) {
                float a = (float)i * 2.0f * (float)M_PI / 6.0f;
                enemies.push_back({ ENEMY_IMP, { std::cos(a) * 18.0f, 0.0f, std::sin(a) * 18.0f }, {0,0,0}, 0.0f, 140.0f, 140.0f, 18.0f, 4.2f, 1.2f, 0.0f, false });
            }
            for (int i = 0; i < 4; i++) {
                float a = (float)i * 2.0f * (float)M_PI / 4.0f + 0.4f;
                enemies.push_back({ ENEMY_HELLHOUND, { std::cos(a) * 22.0f, 0.0f, std::sin(a) * 22.0f }, {0,0,0}, 0.0f, 220.0f, 220.0f, 25.0f, 6.5f, 0.8f, 0.0f, false });
            }
        } else if (wave == 3) {
            // Wave 3: THE CYBER-BARON BOSS + Minions!
            enemies.push_back({ ENEMY_BARON_BOSS, { 0.0f, 0.0f, -22.0f }, {0,0,0}, 0.0f, 3200.0f, 3200.0f, 45.0f, 3.2f, 1.0f, 0.0f, false });
            for (int i = 0; i < 4; i++) {
                float a = (float)i * 2.0f * (float)M_PI / 4.0f;
                enemies.push_back({ ENEMY_IMP, { std::cos(a) * 15.0f, 0.0f, std::sin(a) * 15.0f }, {0,0,0}, 0.0f, 150.0f, 150.0f, 20.0f, 4.5f, 1.5f, 0.0f, false });
            }
        }
    }

    Vec3 getForwardVector() {
        return Vec3(
            -std::sin(camYaw) * std::cos(camPitch),
            std::sin(camPitch),
            -std::cos(camYaw) * std::cos(camPitch)
        );
    }

    Vec3 getRightVector() {
        return Vec3(std::cos(camYaw), 0.0f, -std::sin(camYaw));
    }

    void spawnGibs(Vec3 pos, int count = 16) {
        for (int i = 0; i < count; i++) {
            Particle p;
            p.pos = pos + Vec3((float)(rand() % 40 - 20) * 0.01f, 1.0f, (float)(rand() % 40 - 20) * 0.01f);
            float a = ((float)rand() / (float)RAND_MAX) * 2.0f * (float)M_PI;
            float spd = 4.0f + ((float)rand() / (float)RAND_MAX) * 8.0f;
            p.vel = { std::cos(a) * spd, 3.0f + ((float)rand() / (float)RAND_MAX) * 6.0f, std::sin(a) * spd };
            p.color = (rand() % 2 == 0) ? Vec3(0.85f, 0.05f, 0.05f) : Vec3(0.55f, 0.02f, 0.02f);
            p.size = 0.22f + ((float)rand() / (float)RAND_MAX) * 0.18f;
            p.life = p.maxLife = 1.2f + ((float)rand() / (float)RAND_MAX) * 0.8f;
            p.isGib = true;
            particles.push_back(p);
        }
    }

    void spawnSparks(Vec3 pos, Vec3 color, int count = 12) {
        for (int i = 0; i < count; i++) {
            Particle p;
            p.pos = pos;
            float a = ((float)rand() / (float)RAND_MAX) * 2.0f * (float)M_PI;
            float elev = ((float)rand() / (float)RAND_MAX) * 3.14f;
            float spd = 3.0f + ((float)rand() / (float)RAND_MAX) * 5.0f;
            p.vel = { std::sin(elev) * std::cos(a) * spd, std::cos(elev) * spd, std::sin(elev) * std::sin(a) * spd };
            p.color = color;
            p.size = 0.1f;
            p.life = p.maxLife = 0.3f + ((float)rand() / (float)RAND_MAX) * 0.3f;
            p.isGib = false;
            particles.push_back(p);
        }
    }

    void fireWeapon() {
        if (fireCooldown > 0.0f) return;

        Vec3 fwd = getForwardVector();
        Vec3 right = getRightVector();
        Vec3 eyePos = playerPos + Vec3(0, 1.7f, 0);
        float quadMult = (quadDamageTimer > 0.0f) ? 4.0f : 1.0f;

        if (currentWeapon == WEAPON_SHOTGUN) {
            if (ammoShotgun <= 0) return;
            ammoShotgun--;
            fireCooldown = 0.75f;
            weaponRecoil = 0.35f;
            screenShake = 0.2f;
            audio.playShotgun();

            // Fire 12 pellets in cone
            for (int i = 0; i < 12; i++) {
                float spreadX = ((float)(rand() % 200 - 100)) * 0.0008f;
                float spreadY = ((float)(rand() % 200 - 100)) * 0.0008f;
                Vec3 pelletDir = (fwd + right * spreadX + Vec3(0, spreadY, 0)).normalized();

                // Check hit against enemies
                for (auto& e : enemies) {
                    if (e.isDead) continue;
                    Vec3 toE = (e.pos + Vec3(0, 1.2f, 0)) - eyePos;
                    float d = toE.length();
                    if (d < 35.0f) {
                        float dot = pelletDir.dot(toE.normalized());
                        if (dot > 0.96f) { // Hit!
                            float dmg = 24.0f * quadMult;
                            e.hp -= dmg;
                            e.hitFlash = 0.12f;
                            spawnSparks(e.pos + Vec3(0, 1.2f, 0), Vec3(0.9f, 0.1f, 0.1f), 3);
                            if (e.hp <= 0.0f && !e.isDead) {
                                e.isDead = true;
                                kills++;
                                score += (e.type == ENEMY_BARON_BOSS ? 10000 : 500);
                                spawnGibs(e.pos);
                            }
                        }
                    }
                }
            }
        } else if (currentWeapon == WEAPON_PLASMA) {
            if (ammoPlasma <= 0) return;
            ammoPlasma--;
            fireCooldown = 0.12f;
            weaponRecoil = 0.08f;
            audio.playPlasma();

            // Spawn fast plasma bolt
            projectiles.push_back({
                PROJ_PLASMA,
                eyePos + fwd * 0.5f + right * 0.25f - Vec3(0, 0.2f, 0),
                fwd * 55.0f,
                0.3f,
                28.0f * quadMult,
                2.0f,
                false
            });
        } else if (currentWeapon == WEAPON_ROCKET) {
            if (ammoRockets <= 0) return;
            ammoRockets--;
            fireCooldown = 0.9f;
            weaponRecoil = 0.45f;
            screenShake = 0.25f;
            audio.playRocket();

            // Spawn flying rocket
            projectiles.push_back({
                PROJ_ROCKET,
                eyePos + fwd * 0.8f + right * 0.3f - Vec3(0, 0.2f, 0),
                fwd * 32.0f,
                0.4f,
                180.0f * quadMult,
                4.0f,
                false
            });
        } else if (currentWeapon == WEAPON_CHAINSAW) {
            fireCooldown = 0.15f;
            weaponRecoil = 0.05f;
            screenShake = 0.05f;
            audio.playChainsaw();

            // Check point-blank melee
            for (auto& e : enemies) {
                if (e.isDead) continue;
                if (playerPos.dist(e.pos) < 3.2f) {
                    float dot = fwd.dot((e.pos - playerPos).normalized());
                    if (dot > 0.5f) {
                        e.hp -= 45.0f * quadMult;
                        e.hitFlash = 0.15f;
                        spawnGibs(e.pos + Vec3(0, 1.0f, 0), 4);
                        if (e.hp <= 0.0f && !e.isDead) {
                            e.isDead = true;
                            kills++;
                            score += 800;
                            spawnGibs(e.pos, 25);
                            // Chainsaw kill drops health!
                            playerHp = min(playerMaxHp, playerHp + 15.0f);
                        }
                    }
                }
            }
        }
    }

    void explodeRocket(Vec3 pos, float damage) {
        audio.playExplosion();
        screenShake = 0.5f;
        spawnSparks(pos, Vec3(1.0f, 0.5f, 0.1f), 40);
        spawnGibs(pos, 8);

        // Rocket Jump check (if close to player)
        float distToPlayer = pos.dist(playerPos);
        if (distToPlayer < 4.5f) {
            Vec3 jumpDir = (playerPos - pos).normalized();
            jumpDir.y = max(0.8f, jumpDir.y);
            playerVel += jumpDir * 16.0f;
            damagePlayer(15.0f); // Self-damage
        }

        // AoE damage to enemies
        for (auto& e : enemies) {
            if (e.isDead) continue;
            float d = pos.dist(e.pos + Vec3(0, 1.2f, 0));
            if (d < 7.0f) {
                float falloff = 1.0f - (d / 7.0f);
                float dmg = damage * falloff;
                e.hp -= dmg;
                e.hitFlash = 0.2f;
                e.vel += (e.pos - pos).normalized() * 12.0f;
                if (e.hp <= 0.0f && !e.isDead) {
                    e.isDead = true;
                    kills++;
                    score += (e.type == ENEMY_BARON_BOSS ? 10000 : 800);
                    spawnGibs(e.pos, 30);
                }
            }
        }
    }

    void damagePlayer(float amount) {
        if (gameOver || victory) return;

        // Armor absorbs 60% of damage
        if (playerArmor > 0.0f) {
            float absorbed = amount * 0.6f;
            playerArmor = max(0.0f, playerArmor - absorbed);
            amount -= absorbed;
        }

        playerHp = max(0.0f, playerHp - amount);
        screenDamageFlash = 0.5f;
        screenShake = 0.35f;
        audio.playHit();

        if (playerHp <= 0.0f) {
            gameOver = true;
            captureMouse(false);
        }
    }

    void update(float dt) {
        globalTime += dt;

        // Cooldowns
        if (fireCooldown > 0.0f) fireCooldown -= dt;
        if (weaponRecoil > 0.0f) weaponRecoil = max(0.0f, weaponRecoil - dt * 2.5f);
        if (screenDamageFlash > 0.0f) screenDamageFlash = max(0.0f, screenDamageFlash - dt * 1.8f);
        if (screenShake > 0.0f) screenShake = max(0.0f, screenShake - dt * 2.2f);
        if (quadDamageTimer > 0.0f) quadDamageTimer = max(0.0f, quadDamageTimer - dt);

        if (gameOver || victory) return;

        // Player Movement (Quake style fast strafing)
        Vec3 moveDir(0, 0, 0);
        float sinY = std::sin(camYaw), cosY = std::cos(camYaw);

        if (keys['W']) { moveDir.x -= sinY; moveDir.z -= cosY; }
        if (keys['S']) { moveDir.x += sinY; moveDir.z += cosY; }
        if (keys['A']) { moveDir.x -= cosY; moveDir.z += sinY; strafeTilt = min(0.06f, strafeTilt + dt * 0.5f); }
        if (keys['D']) { moveDir.x += cosY; moveDir.z -= sinY; strafeTilt = max(-0.06f, strafeTilt - dt * 0.5f); }
        if (!keys['A'] && !keys['D']) {
            strafeTilt *= 0.85f;
        }

        bool isMoving = moveDir.lengthSq() > 0.001f;
        float moveSpeed = (keys[VK_SHIFT] ? 14.5f : 9.5f);

        if (isMoving) {
            moveDir = moveDir.normalized();
            playerPos.x += moveDir.x * moveSpeed * dt;
            playerPos.z += moveDir.z * moveSpeed * dt;
            headBob += dt * (keys[VK_SHIFT] ? 16.0f : 11.0f);
        }

        // Apply external velocity (rocket jumps / knockbacks)
        playerPos += playerVel * dt;
        playerVel.x *= 0.90f;
        playerVel.z *= 0.90f;

        // Jump & Gravity
        if (keys[VK_SPACE] && isGrounded) {
            playerVel.y = 9.0f;
            isGrounded = false;
        }

        if (!isGrounded) {
            playerVel.y -= 25.0f * dt;
            playerPos.y += playerVel.y * dt;
            if (playerPos.y <= 0.0f) {
                playerPos.y = 0.0f;
                playerVel.y = 0.0f;
                isGrounded = true;
            }
        }

        // Arena boundary clamp (Square Arena: -28 to +28)
        playerPos.x = max(-27.0f, min(27.0f, playerPos.x));
        playerPos.z = max(-27.0f, min(27.0f, playerPos.z));

        // Weapon Switch Keys
        if (keys['1']) currentWeapon = WEAPON_SHOTGUN;
        if (keys['2']) currentWeapon = WEAPON_PLASMA;
        if (keys['3']) currentWeapon = WEAPON_ROCKET;
        if (keys['4']) currentWeapon = WEAPON_CHAINSAW;

        // Update Projectiles
        for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
            auto& p = projectiles[i];
            p.pos += p.vel * dt;
            p.life -= dt;

            // Spawn rocket trail
            if (p.type == PROJ_ROCKET) {
                spawnSparks(p.pos, Vec3(0.5f, 0.5f, 0.5f), 1);
            }

            // Wall or floor collision
            bool hitWall = (p.pos.y <= 0.0f || std::abs(p.pos.x) >= 28.0f || std::abs(p.pos.z) >= 28.0f);

            if (p.type == PROJ_ROCKET && hitWall) {
                explodeRocket(p.pos, p.damage);
                projectiles.erase(projectiles.begin() + i);
                continue;
            }

            if (p.life <= 0.0f || hitWall) {
                spawnSparks(p.pos, Vec3(0.2f, 0.8f, 1.0f), 8);
                projectiles.erase(projectiles.begin() + i);
                continue;
            }

            // Hit Player (Enemy projectiles)
            if (p.isEnemy) {
                if (p.pos.dist(playerPos + Vec3(0, 1.2f, 0)) < 1.2f) {
                    damagePlayer(p.damage);
                    spawnSparks(p.pos, Vec3(1.0f, 0.2f, 0.1f), 12);
                    projectiles.erase(projectiles.begin() + i);
                    continue;
                }
            } else {
                // Hit Enemies (Player projectiles)
                bool hitEnemy = false;
                for (auto& e : enemies) {
                    if (e.isDead) continue;
                    if (p.pos.dist(e.pos + Vec3(0, 1.2f, 0)) < (e.type == ENEMY_BARON_BOSS ? 2.5f : 1.2f)) {
                        hitEnemy = true;
                        if (p.type == PROJ_ROCKET) {
                            explodeRocket(p.pos, p.damage);
                        } else {
                            e.hp -= p.damage;
                            e.hitFlash = 0.15f;
                            spawnSparks(p.pos, Vec3(0.1f, 0.9f, 1.0f), 10);
                            audio.playHit();
                            if (e.hp <= 0.0f && !e.isDead) {
                                e.isDead = true;
                                kills++;
                                score += (e.type == ENEMY_BARON_BOSS ? 10000 : 500);
                                spawnGibs(e.pos);
                            }
                        }
                        break;
                    }
                }
                if (hitEnemy && p.type != PROJ_ROCKET) {
                    projectiles.erase(projectiles.begin() + i);
                    continue;
                }
            }
        }

        // Update Pickups
        for (auto& pick : pickups) {
            if (!pick.active) {
                pick.respawnTimer -= dt;
                if (pick.respawnTimer <= 0.0f) pick.active = true;
                continue;
            }

            if (playerPos.dist(pick.pos) < 1.8f) {
                pick.active = false;
                pick.respawnTimer = 20.0f; // 20s respawn
                audio.playPickup();

                switch (pick.type) {
                case PICKUP_HEALTH:
                    playerHp = min(playerMaxHp, playerHp + 35.0f);
                    break;
                case PICKUP_ARMOR:
                    playerArmor = min(playerMaxArmor, playerArmor + 35.0f);
                    break;
                case PICKUP_AMMO_SG:
                    ammoShotgun += 16;
                    break;
                case PICKUP_AMMO_PLASMA:
                    ammoPlasma += 60;
                    break;
                case PICKUP_AMMO_ROCKET:
                    ammoRockets += 6;
                    break;
                case PICKUP_QUAD:
                    quadDamageTimer = 15.0f;
                    break;
                }
            }
        }

        // Update Enemies AI
        bool allDead = true;
        for (auto& e : enemies) {
            if (e.isDead) continue;
            allDead = false;

            if (e.hitFlash > 0.0f) e.hitFlash -= dt;

            Vec3 toPlayer = playerPos - e.pos;
            toPlayer.y = 0;
            float dist = toPlayer.length();
            if (dist > 0.001f) {
                e.yaw = std::atan2(toPlayer.x, toPlayer.z) + (float)M_PI;
            }

            e.attackTimer -= dt;

            if (e.type == ENEMY_IMP) {
                if (dist > 12.0f) {
                    e.pos += toPlayer.normalized() * e.speed * dt;
                } else if (e.attackTimer <= 0.0f) {
                    // Fireball attack
                    e.attackTimer = 2.0f;
                    Vec3 dir = (playerPos + Vec3(0, 1.2f, 0) - (e.pos + Vec3(0, 1.2f, 0))).normalized();
                    projectiles.push_back({
                        PROJ_ENEMY_FIREBALL,
                        e.pos + Vec3(0, 1.2f, 0),
                        dir * 18.0f,
                        0.4f,
                        18.0f,
                        4.0f,
                        true
                    });
                }
            } else if (e.type == ENEMY_HELLHOUND) {
                // Fast melee rush
                if (dist > 1.8f) {
                    e.pos += toPlayer.normalized() * e.speed * dt;
                } else if (e.attackTimer <= 0.0f) {
                    e.attackTimer = 1.0f;
                    damagePlayer(e.atk);
                }
            } else if (e.type == ENEMY_BARON_BOSS) {
                // Boss Baron
                if (dist > 6.0f) {
                    e.pos += toPlayer.normalized() * e.speed * dt;
                }
                if (e.attackTimer <= 0.0f) {
                    e.attackTimer = 1.4f;
                    // Spread of 3 plasma orbs
                    Vec3 f = (playerPos + Vec3(0, 1.4f, 0) - (e.pos + Vec3(0, 2.5f, 0))).normalized();
                    Vec3 r(-f.z, 0, f.x);
                    for (int off = -1; off <= 1; off++) {
                        Vec3 d = (f + r * (off * 0.2f)).normalized();
                        projectiles.push_back({
                            PROJ_ENEMY_PLASMA,
                            e.pos + Vec3(0, 2.5f, 0),
                            d * 22.0f,
                            0.5f,
                            25.0f,
                            4.0f,
                            true
                        });
                    }
                }
            }

            // Knockback
            e.pos += e.vel * dt;
            e.vel = e.vel * 0.85f;
        }

        // Wave Progression
        if (allDead && !victory) {
            if (currentWave < 3) {
                spawnWave(currentWave + 1);
            } else {
                victory = true;
                captureMouse(false);
            }
        }

        // Update Particles & Gibs
        for (int i = (int)particles.size() - 1; i >= 0; i--) {
            auto& p = particles[i];
            p.life -= dt;
            if (p.life <= 0.0f) {
                particles.erase(particles.begin() + i);
                continue;
            }
            p.pos += p.vel * dt;
            p.vel.y -= 18.0f * dt; // Gravity
            // Floor bounce
            if (p.pos.y <= 0.05f) {
                p.pos.y = 0.05f;
                p.vel.y = -p.vel.y * 0.45f;
                p.vel.x *= 0.65f;
                p.vel.z *= 0.65f;
            }
        }
    }

    // --- 3D Geometry Primitives ---
    void drawBox(float sx, float sy, float sz) {
        float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
        glBegin(GL_QUADS);
        // Front
        glNormal3f(0, 0, 1);
        glVertex3f(-hx, -hy, hz); glVertex3f(hx, -hy, hz);
        glVertex3f(hx, hy, hz); glVertex3f(-hx, hy, hz);
        // Back
        glNormal3f(0, 0, -1);
        glVertex3f(hx, -hy, -hz); glVertex3f(-hx, -hy, -hz);
        glVertex3f(-hx, hy, -hz); glVertex3f(hx, hy, -hz);
        // Top
        glNormal3f(0, 1, 0);
        glVertex3f(-hx, hy, hz); glVertex3f(hx, hy, hz);
        glVertex3f(hx, hy, -hz); glVertex3f(-hx, hy, -hz);
        // Bottom
        glNormal3f(0, -1, 0);
        glVertex3f(-hx, -hy, -hz); glVertex3f(hx, -hy, -hz);
        glVertex3f(hx, -hy, hz); glVertex3f(-hx, -hy, hz);
        // Left
        glNormal3f(-1, 0, 0);
        glVertex3f(-hx, -hy, -hz); glVertex3f(-hx, -hy, hz);
        glVertex3f(-hx, hy, hz); glVertex3f(-hx, hy, -hz);
        // Right
        glNormal3f(1, 0, 0);
        glVertex3f(hx, -hy, hz); glVertex3f(hx, -hy, -hz);
        glVertex3f(hx, hy, -hz); glVertex3f(hx, hy, hz);
        glEnd();
    }

    void drawArena() {
        // Floor (Dark metallic industrial grid)
        glColor3f(0.12f, 0.12f, 0.14f);
        glPushMatrix();
        glTranslatef(0, -0.1f, 0);
        drawBox(60.0f, 0.2f, 60.0f);
        glPopMatrix();

        // Floor Grid Lines
        glDisable(GL_LIGHTING);
        glColor3f(0.25f, 0.15f, 0.12f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (int i = -28; i <= 28; i += 4) {
            glVertex3f((float)i, 0.02f, -28.0f); glVertex3f((float)i, 0.02f, 28.0f);
            glVertex3f(-28.0f, 0.02f, (float)i); glVertex3f(28.0f, 0.02f, (float)i);
        }
        glEnd();

        // Perimeter Walls
        glColor3f(0.20f, 0.18f, 0.22f);
        glPushMatrix();
        glTranslatef(0, 4.0f, -28.0f); drawBox(60.0f, 8.0f, 1.0f); glPopMatrix();
        glPushMatrix();
        glTranslatef(0, 4.0f, 28.0f); drawBox(60.0f, 8.0f, 1.0f); glPopMatrix();
        glPushMatrix();
        glTranslatef(-28.0f, 4.0f, 0); drawBox(1.0f, 8.0f, 60.0f); glPopMatrix();
        glPushMatrix();
        glTranslatef(28.0f, 4.0f, 0); drawBox(1.0f, 8.0f, 60.0f); glPopMatrix();

        // Pillars
        for (int x = -16; x <= 16; x += 32) {
            for (int z = -16; z <= 16; z += 32) {
                glPushMatrix();
                glTranslatef((float)x, 4.0f, (float)z);
                glColor3f(0.35f, 0.25f, 0.22f);
                drawBox(2.2f, 8.0f, 2.2f);
                // Torch on pillar
                glTranslatef(0, 2.0f, 1.2f);
                glColor3f(1.0f, 0.5f, 0.1f);
                drawBox(0.4f, 0.6f, 0.4f);
                glPopMatrix();
            }
        }
        glEnable(GL_LIGHTING);
    }

    void drawPickups() {
        for (const auto& pick : pickups) {
            if (!pick.active) continue;
            glPushMatrix();
            glTranslatef(pick.pos.x, pick.pos.y + std::sin(globalTime * 4.0f) * 0.15f, pick.pos.z);
            glRotatef(globalTime * 90.0f, 0, 1, 0);

            switch (pick.type) {
            case PICKUP_HEALTH:
                glColor3f(0.9f, 0.1f, 0.1f);
                drawBox(0.8f, 0.8f, 0.8f);
                glColor3f(1.0f, 1.0f, 1.0f); // White cross
                drawBox(0.25f, 0.85f, 0.25f);
                break;
            case PICKUP_ARMOR:
                glColor3f(0.1f, 0.7f, 1.0f);
                drawBox(0.9f, 0.7f, 0.5f);
                break;
            case PICKUP_AMMO_SG:
                glColor3f(0.8f, 0.3f, 0.1f);
                drawBox(0.6f, 0.6f, 0.6f);
                break;
            case PICKUP_AMMO_PLASMA:
                glColor3f(0.2f, 0.9f, 0.9f);
                drawBox(0.5f, 0.8f, 0.5f);
                break;
            case PICKUP_AMMO_ROCKET:
                glColor3f(0.3f, 0.6f, 0.2f);
                drawBox(0.7f, 0.5f, 0.7f);
                break;
            case PICKUP_QUAD:
                glColor3f(0.8f, 0.2f, 1.0f); // Glowing Purple Quad Damage
                drawBox(0.9f, 0.9f, 0.9f);
                break;
            }
            glPopMatrix();
        }
    }

    void drawEnemies() {
        for (const auto& e : enemies) {
            if (e.isDead) continue;
            glPushMatrix();
            glTranslatef(e.pos.x, e.pos.y, e.pos.z);
            glRotatef(e.yaw * 180.0f / (float)M_PI, 0, 1, 0);

            if (e.hitFlash > 0.0f) {
                glColor3f(1.0f, 1.0f, 1.0f);
            }

            if (e.type == ENEMY_IMP) {
                // Imp (Brown/Red Demon)
                if (e.hitFlash <= 0.0f) glColor3f(0.65f, 0.25f, 0.15f);
                glTranslatef(0, 1.1f, 0);
                drawBox(0.8f, 1.2f, 0.6f);
                // Glowing Orange Eyes
                glColor3f(1.0f, 0.6f, 0.0f);
                glPushMatrix();
                glTranslatef(-0.2f, 0.4f, 0.32f); drawBox(0.12f, 0.12f, 0.05f);
                glTranslatef(0.4f, 0.0f, 0.0f); drawBox(0.12f, 0.12f, 0.05f);
                glPopMatrix();
                // Horns
                glColor3f(0.2f, 0.15f, 0.1f);
                glPushMatrix();
                glTranslatef(-0.35f, 0.75f, 0); drawBox(0.15f, 0.4f, 0.15f);
                glTranslatef(0.7f, 0, 0); drawBox(0.15f, 0.4f, 0.15f);
                glPopMatrix();
            } else if (e.type == ENEMY_HELLHOUND) {
                // Hellhound (Fast Quadruped)
                if (e.hitFlash <= 0.0f) glColor3f(0.35f, 0.15f, 0.15f);
                glTranslatef(0, 0.7f, 0);
                drawBox(0.9f, 0.7f, 1.6f);
                // Jaws
                glColor3f(0.85f, 0.1f, 0.1f);
                glPushMatrix();
                glTranslatef(0, 0.1f, 0.9f); drawBox(0.5f, 0.35f, 0.5f);
                glPopMatrix();
            } else if (e.type == ENEMY_BARON_BOSS) {
                // Cyber-Baron Boss (Massive Horned Cyber-Demon)
                if (e.hitFlash <= 0.0f) glColor3f(0.55f, 0.12f, 0.12f);
                glTranslatef(0, 2.4f, 0);
                drawBox(2.2f, 2.6f, 1.8f);
                // Cybernetic Cannon Arm
                glColor3f(0.2f, 0.25f, 0.3f);
                glPushMatrix();
                glTranslatef(1.6f, 0.0f, 0.5f);
                drawBox(0.7f, 0.7f, 2.0f);
                glPopMatrix();
                // Giant Demon Horns
                glColor3f(0.85f, 0.75f, 0.4f);
                glPushMatrix();
                glTranslatef(-1.0f, 1.5f, 0.2f); drawBox(0.35f, 1.2f, 0.35f);
                glTranslatef(2.0f, 0.0f, 0.0f); drawBox(0.35f, 1.2f, 0.35f);
                glPopMatrix();
            }

            glPopMatrix();
        }
    }

    void drawProjectiles() {
        glDisable(GL_LIGHTING);
        for (const auto& p : projectiles) {
            glPushMatrix();
            glTranslatef(p.pos.x, p.pos.y, p.pos.z);
            if (p.type == PROJ_ROCKET) {
                glColor3f(0.8f, 0.3f, 0.1f);
                drawBox(0.25f, 0.25f, 0.8f);
            } else if (p.type == PROJ_PLASMA) {
                glColor3f(0.1f, 0.85f, 1.0f);
                drawBox(0.35f, 0.35f, 0.35f);
            } else if (p.type == PROJ_ENEMY_FIREBALL) {
                glColor3f(1.0f, 0.4f, 0.0f);
                drawBox(0.45f, 0.45f, 0.45f);
            } else if (p.type == PROJ_ENEMY_PLASMA) {
                glColor3f(0.1f, 1.0f, 0.3f);
                drawBox(0.6f, 0.6f, 0.6f);
            }
            glPopMatrix();
        }
        glEnable(GL_LIGHTING);
    }

    void drawParticles() {
        glDisable(GL_LIGHTING);
        for (const auto& p : particles) {
            glPushMatrix();
            glTranslatef(p.pos.x, p.pos.y, p.pos.z);
            glColor3fv(&p.color.x);
            drawBox(p.size, p.size, p.size);
            glPopMatrix();
        }
        glEnable(GL_LIGHTING);
    }

    // --- 3D First-Person Weapon Viewmodel ---
    void drawWeaponViewModel() {
        glClear(GL_DEPTH_BUFFER_BIT); // Always draw weapon on top
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluPerspective(55.0, (double)screenW / (double)(screenH > 0 ? screenH : 1), 0.05, 20.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        // Position in lower right with recoil
        float bobX = std::sin(headBob * 0.5f) * 0.015f;
        float bobY = -std::abs(std::cos(headBob)) * 0.015f;
        glTranslatef(0.28f + bobX, -0.22f + bobY, -0.55f + weaponRecoil * 0.25f);
        glRotatef(weaponRecoil * 15.0f, 1.0f, 0.0f, 0.0f);

        if (currentWeapon == WEAPON_SHOTGUN) {
            // Super Shotgun (Double Barrel)
            glColor3f(0.2f, 0.22f, 0.26f); // Metal Barrels
            glPushMatrix();
            glTranslatef(-0.035f, 0, 0); drawBox(0.045f, 0.045f, 0.45f);
            glTranslatef(0.07f, 0, 0); drawBox(0.045f, 0.045f, 0.45f);
            glPopMatrix();
            // Wooden Stock
            glColor3f(0.45f, 0.25f, 0.12f);
            glPushMatrix();
            glTranslatef(0, -0.06f, 0.25f); drawBox(0.09f, 0.14f, 0.25f);
            glPopMatrix();
            // Muzzle flash
            if (fireCooldown > 0.65f) {
                glDisable(GL_LIGHTING);
                glColor3f(1.0f, 0.8f, 0.2f);
                glPushMatrix();
                glTranslatef(0, 0, -0.25f); drawBox(0.18f, 0.18f, 0.18f);
                glPopMatrix();
                glEnable(GL_LIGHTING);
            }
        } else if (currentWeapon == WEAPON_PLASMA) {
            // Plasma Rifle (Cyan glowing coils)
            glColor3f(0.22f, 0.25f, 0.3f);
            drawBox(0.12f, 0.14f, 0.45f);
            // Glowing Energy Coils
            glDisable(GL_LIGHTING);
            glColor3f(0.1f, 0.9f, 1.0f);
            glPushMatrix();
            glTranslatef(0, 0.05f, -0.05f); drawBox(0.14f, 0.05f, 0.2f);
            glPopMatrix();
            glEnable(GL_LIGHTING);
        } else if (currentWeapon == WEAPON_ROCKET) {
            // Rocket Launcher (Heavy green tube)
            glColor3f(0.25f, 0.35f, 0.2f);
            drawBox(0.16f, 0.16f, 0.6f);
            // Loaded rocket nose tip
            glColor3f(0.8f, 0.2f, 0.1f);
            glPushMatrix();
            glTranslatef(0, 0, -0.32f); drawBox(0.12f, 0.12f, 0.12f);
            glPopMatrix();
        } else if (currentWeapon == WEAPON_CHAINSAW) {
            // Chainsaw (Aggressive revving blade)
            glColor3f(0.85f, 0.65f, 0.1f); // Yellow casing
            drawBox(0.14f, 0.16f, 0.35f);
            // Teeth blade
            glColor3f(0.5f, 0.52f, 0.55f);
            glPushMatrix();
            glTranslatef(0, 0, -0.3f + std::sin(globalTime * 60.0f) * 0.02f);
            drawBox(0.04f, 0.12f, 0.35f);
            glPopMatrix();
        }

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    // --- 2D Orthographic HUD (Retro Doom/Quake Style) ---
    void renderHUD() {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, screenW, screenH, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        int cx = screenW / 2;
        int cy = screenH / 2;

        // 1. Crosshair in center
        glColor3f(0.9f, 0.9f, 0.9f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2i(cx - 7, cy); glVertex2i(cx - 2, cy);
        glVertex2i(cx + 3, cy); glVertex2i(cx + 8, cy);
        glVertex2i(cx, cy - 7); glVertex2i(cx, cy - 2);
        glVertex2i(cx, cy + 3); glVertex2i(cx, cy + 8);
        glEnd();

        // 2. Red Damage Flash
        if (screenDamageFlash > 0.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.9f, 0.05f, 0.05f, screenDamageFlash * 0.6f);
            glRecti(0, 0, screenW, screenH);
            glDisable(GL_BLEND);
        }

        // 3. Quad Damage Alert Banner
        if (quadDamageTimer > 0.0f) {
            glColor3f(0.85f, 0.15f, 1.0f);
            glRecti(cx - 160, 20, cx + 160, 48);
        }

        // 4. Boss Health Bar (Top Screen if Baron Boss is alive)
        for (const auto& e : enemies) {
            if (e.type == ENEMY_BARON_BOSS && !e.isDead) {
                int bW = 540, bH = 20;
                int bX = cx - bW / 2, bY = 60;
                float pct = max(0.0f, e.hp / e.maxHp);
                glColor3f(0.1f, 0.1f, 0.15f);
                glRecti(bX - 4, bY - 4, bX + bW + 4, bY + bH + 4);
                glColor3f(0.95f, 0.1f, 0.1f);
                glRecti(bX, bY, bX + (int)(bW * pct), bY + bH);
            }
        }

        // 5. Classic Status Bar (Bottom)
        int barH = 50;
        int barY = screenH - barH;
        glColor3f(0.08f, 0.09f, 0.12f);
        glRecti(0, barY, screenW, screenH);
        glColor3f(0.25f, 0.28f, 0.35f);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        glVertex2i(0, barY); glVertex2i(screenW, barY);
        glEnd();

        // HEALTH Segment
        int hpPct = (int)((playerHp / playerMaxHp) * 100.0f);
        glColor3f(0.9f, 0.15f, 0.15f);
        glRecti(25, barY + 10, 25 + max(0, hpPct * 2), barY + 38);

        // ARMOR Segment
        int armPct = (int)((playerArmor / playerMaxArmor) * 100.0f);
        glColor3f(0.15f, 0.65f, 0.95f);
        glRecti(260, barY + 10, 260 + max(0, armPct * 2), barY + 38);

        // AMMO Display (Current Weapon)
        int curAmmo = (currentWeapon == WEAPON_SHOTGUN ? ammoShotgun :
                      (currentWeapon == WEAPON_PLASMA ? ammoPlasma :
                      (currentWeapon == WEAPON_ROCKET ? ammoRockets : 999)));

        glColor3f(1.0f, 0.85f, 0.15f);
        int ammoWidth = min(200, curAmmo * 4);
        glRecti(cx + 60, barY + 10, cx + 60 + ammoWidth, barY + 38);

        // Weapon Selector Slots [1] [2] [3] [4]
        for (int i = 0; i < WEAPON_COUNT; i++) {
            int slotX = screenW - 190 + i * 45;
            int slotY = barY + 10;
            if (currentWeapon == i) {
                glColor3f(1.0f, 0.85f, 0.2f); // Highlight active
            } else {
                glColor3f(0.25f, 0.3f, 0.4f);
            }
            glRecti(slotX, slotY, slotX + 38, slotY + 30);
        }

        // 6. Victory & Game Over Screens
        if (victory) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.0f, 0.0f, 0.0f, 0.75f);
            glRecti(0, 0, screenW, screenH);
            glColor3f(1.0f, 0.85f, 0.1f);
            glRecti(cx - 200, cy - 45, cx + 200, cy + 45);
            glDisable(GL_BLEND);
        } else if (gameOver) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
            glRecti(0, 0, screenW, screenH);
            glColor3f(0.85f, 0.1f, 0.1f);
            glRecti(cx - 200, cy - 45, cx + 200, cy + 45);
            glDisable(GL_BLEND);
        }

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
    }

    void render() {
        glClearColor(0.06f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, screenW, screenH);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = (float)screenW / (float)(screenH > 0 ? screenH : 1);
        gluPerspective(75.0, (double)aspect, 0.1, 500.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Screen Shake
        float shakeX = (screenShake > 0.0f) ? ((rand() % 100 - 50) * 0.006f * screenShake) : 0.0f;
        float shakeY = (screenShake > 0.0f) ? ((rand() % 100 - 50) * 0.006f * screenShake) : 0.0f;

        // Camera Look Vector
        Vec3 fwd = getForwardVector();
        Vec3 eye = playerPos + Vec3(shakeX, 1.7f + std::sin(headBob) * 0.08f + shakeY, 0.0f);
        Vec3 target = eye + fwd;

        gluLookAt(
            (double)eye.x, (double)eye.y, (double)eye.z,
            (double)target.x, (double)target.y, (double)target.z,
            (double)strafeTilt, 1.0, 0.0
        );

        // Light updates in world
        float lightPos[] = { 0.0f, 12.0f, 0.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

        // Render World
        drawArena();
        drawPickups();
        drawEnemies();
        drawProjectiles();
        drawParticles();

        // Render 1st-Person Weapon
        drawWeaponViewModel();

        // Render 2D HUD
        renderHUD();

        SwapBuffers(hDC);
    }
};

DreadfallEngine g_game;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        g_game.screenW = LOWORD(lParam);
        g_game.screenH = HIWORD(lParam);
        return 0;

    case WM_KEYDOWN:
        if (wParam < 256) g_game.keys[wParam] = true;
        if (wParam == VK_ESCAPE) {
            g_game.captureMouse(!g_game.mouseCaptured);
        }
        if (wParam == 'R' && (g_game.gameOver || g_game.victory)) {
            g_game.playerHp = g_game.playerMaxHp;
            g_game.playerArmor = 50.0f;
            g_game.ammoShotgun = 32;
            g_game.ammoPlasma = 120;
            g_game.ammoRockets = 15;
            g_game.gameOver = false;
            g_game.victory = false;
            g_game.spawnWave(1);
            g_game.captureMouse(true);
        }
        return 0;

    case WM_KEYUP:
        if (wParam < 256) g_game.keys[wParam] = false;
        return 0;

    case WM_LBUTTONDOWN:
        if (!g_game.mouseCaptured) {
            g_game.captureMouse(true);
        } else {
            g_game.fireWeapon();
        }
        return 0;

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) {
            g_game.currentWeapon = (WeaponType)((g_game.currentWeapon + 1) % WEAPON_COUNT);
        } else if (delta < 0) {
            g_game.currentWeapon = (WeaponType)((g_game.currentWeapon + WEAPON_COUNT - 1) % WEAPON_COUNT);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_game.mouseCaptured) {
            POINT center = { g_game.screenW / 2, g_game.screenH / 2 };
            ClientToScreen(hWnd, &center);

            POINT cur;
            GetCursorPos(&cur);
            int dx = cur.x - center.x;
            int dy = cur.y - center.y;

            if (dx != 0 || dy != 0) {
                g_game.camYaw -= (float)dx * 0.0032f;
                g_game.camPitch -= (float)dy * 0.0032f;
                // Clamp pitch so player cannot flip upside down
                g_game.camPitch = max(-1.45f, min(1.45f, g_game.camPitch));
                SetCursorPos(center.x, center.y);
            }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DreadfallClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w = 1280, h = 720;
    int x = (sw - w) / 2, y = (sh - h) / 2;

    HWND hWnd = CreateWindowEx(
        0, "DreadfallClass", "DREADFALL 3D - Boomer Shooter",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, w, h, NULL, NULL, hInstance, NULL
    );

    g_game.init(hWnd);

    LARGE_INTEGER freq, lastTime, curTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            QueryPerformanceCounter(&curTime);
            float dt = (float)(curTime.QuadPart - lastTime.QuadPart) / (float)freq.QuadPart;
            lastTime = curTime;
            dt = min(dt, 0.05f);

            g_game.update(dt);
            g_game.render();
        }
    }

    return 0;
}
