#include "glut.h" // GLUTライブラリ
#include <GL/gl.h> // OpenGLライブラリ
#define _USE_MATH_DEFINES // WindowsでM_PIを使うため
#include <math.h> // 数学関数
#include <cmath> // C++の数学関数
#include <vector> // 動的配列
#include <iostream> // デバッグ用
#include <random>    // 乱数生成エンジン (std::mt19937)
#include <chrono>    // 乱数シード用

// カメラ変数
float cameraX = 0.0f;     // カメラX座標
float cameraY = 1.75f;    // カメラY座標 (視点の高さ)
float cameraZ = 0.0f;     // カメラZ座標
float cameraRotationY = 0.0f; // カメラのヨー (水平回転)
float cameraAngleX = 0.0f; // カメラのピッチ (垂直回転) - 初期値は正面
float lastMouseX, lastMouseY; // マウスの最終座標
bool mouseDragging = false; // マウスドラッグ中か

// キーボード状態配列
bool keyStates[256] = { false }; // 通常キー用
bool specialKeyStates[256] = { false }; // 特殊キー (GLUT_KEY_UPなど)用

// 乱数生成器設定
std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count()); // シード設定

// 指定範囲でランダムな浮動小数点数を生成するヘルパー関数
float getRandomFloat(float min, float max) {
    // Mersenne Twisterエンジンで0.0から1.0の乱数を生成
    float random_0_to_1 = static_cast<float>(rng()) / rng.max();
    // 目的の範囲 [min, max] にスケールしてシフト
    return min + (max - min) * random_0_to_1;
}

struct Object {
    float x, y, z; // 位置
    float r, g, b; // 色
    float radius;  // サイズ
};

std::vector<Object> objects; // シーン内の静的オブジェクト (現在空)

// ランタンの状態 - 各ランタン用
struct KomLoyLantern {
    float x, y, z; // 位置
    float velX, velY, velZ; // 移動速度
    float currentFlameAnimation; // 個別の炎アニメーション時間
    float corePulsation; // 個別の炎の核の脈動値
};

std::vector<KomLoyLantern> lanterns; // 複数のランタンを保持するベクター
const int NUM_LANTERNS = 1500; // 生成するランタンの数

// ランタンの静的パーツ用ディスプレイリスト
GLuint lanternDisplayList;

struct FlamePolygon {
    float scale_x, scale_y; // スケール
    float rotation; // 回転
    float alpha; // 透明度
    float animation_offset; // 各ポリゴン固有のアニメーションオフセット
};

std::vector<FlamePolygon> flamePolygons; // 炎の一般的な形状/挙動を定義

// 星空の変数
struct Star {
    float x, y, z; // 星の位置
};
std::vector<Star> stars; // 星のベクター
const int NUM_STARS = 1000; // 星の数

// 関数プロトタイプ (宣言)
void drawFlame(float flameAnimTime, float corePulse); // 炎を描画
void drawHook(); // フックを描画
void drawLanternFrame(); // ランタンのフレームを描画
void drawLanternCover(); // ランタンのカバーを描画
void drawLanternRoof(); // ランタンの屋根を描画
void drawSingleLantern(const KomLoyLantern& l); // 個々のランタンを描画
void drawGround(); // 地面を描画
void drawStars(); // 星を描画

// 炎を描画する関数 (個々のランタンのアニメーション時間を使用)
void drawFlame(float flameAnimTime, float corePulse) {
    // 現在のライティング状態を保存
    GLboolean wasLightingEnabled;
    glGetBooleanv(GL_LIGHTING, &wasLightingEnabled);

    // --- 炎の核を描画 ---
    glEnable(GL_LIGHTING); // 核の発光のためにライティングを有効化
    glDepthMask(GL_TRUE); // 核がデプスバッファに書き込むようにする

    glPushMatrix();
    // ランタンの底より少し下に核を配置
    glTranslatef(0.0f, -0.65f, 0.0f);

    // 各核の脈動に基づいてサイズと色をアニメーション
    float corePulseScale = 0.1f + corePulse * 0.05f; // サイズの小さな脈動
    float coreR = 1.0f;
    float coreG = 0.4f + corePulse * 0.6f; // 脈動でより明るい黄色に
    float coreB = 0.1f;

    glColor4f(coreR, coreG, coreB, 1.0f); // 不透明な核の色
    glScalef(corePulseScale, corePulseScale * 1.5f, corePulseScale); // 縦長の形状、脈動付き

    // 核を発光させるためのエミッションプロパティを設定
    GLfloat emission[] = { coreR, coreG * 0.7f, coreB * 0.5f, 1.0f }; // 温かい黄色の光
    glMaterialfv(GL_FRONT, GL_EMISSION, emission);

    glutSolidSphere(0.5, 10, 10); // 核を球体として描画

    // 他のオブジェクトが発光しないようにエミッションをリセット
    GLfloat no_emission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
    glPopMatrix();

    // --- 揺らめく炎のポリゴンを描画 ---
    glDisable(GL_LIGHTING); // 炎ポリゴンは自己発光するためライティングを無効化
    glDepthMask(GL_FALSE);    // 適切なブレンドのためにデプス書き込みを無効化 (ポリゴン同士が遮蔽し合うのを防ぐ)
    // 発光効果のために加算ブレンドを使用。これにより重なる透明部分が明るくなる。
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& p : flamePolygons) {
        glPushMatrix();
        // 炎のポリゴンが核の中心Y座標と揃うように配置
        glTranslatef(0.0f, -0.65f, 0.0f); // 炎の基点は核の中心Yから開始
        glRotatef(p.rotation, 0.0f, 0.0f, 1.0f); // 点滅効果のための回転
        // ランタン内に炎が収まるようにスケール調整
        glScalef(p.scale_x * 0.8f, p.scale_y * 0.8f, 1.0f); // 全体的な炎のサイズを縮小

        // 3D効果と滑らかな見た目のために複数レイヤーを描画
        for (int i = 0; i < 10; ++i) {
            glPushMatrix();
            // より有機的な見た目のためにY軸周りにランダムな揺れで回転
            glRotatef(i * 18.0f + sin(p.animation_offset * 3.0f) * 5.0f, 0.0f, 1.0f, 0.0f);

            glBegin(GL_TRIANGLES);
            // 炎の先端 (より明るく、黄色みが強く、レイヤーごとにフェードアウト)
            // 基点がローカルY=0になるようにY座標を調整
            glColor4f(1.0f, 1.0f, 0.5f, p.alpha * (0.8f - 0.5f * (float)i / 9.0f));
            glVertex3f(0.0f, 0.75f, 0.0f); // 先端

            // 炎の基点 (赤みがかったオレンジ、透明度が低く、核の脈動で明るくなる)
            // 基点がローカルY=0になるようにY座標を調整
            glColor4f(1.0f, 0.4f + corePulse * 0.3f, 0.1f, p.alpha * (0.2f + 0.3f * (float)i / 9.0f));
            glVertex3f(-0.2f, 0.0f, 0.0f); // 左下
            glVertex3f(0.2f, 0.0f, 0.0f);  // 右下
            glEnd();
            glPopMatrix();
        }

        glPopMatrix();
    }

    glDepthMask(GL_TRUE);    // 透明部分の描画後にデプス書き込みを再有効化

    // ライティング状態を復元
    if (wasLightingEnabled) {
        glEnable(GL_LIGHTING);
    }
    else {
        glDisable(GL_LIGHTING);
    }
    // 他の透明オブジェクトに影響を与えないようにデフォルトのブレンド関数を復元
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ランタン上部のフックを描画する関数
void drawHook() {
    glDisable(GL_LIGHTING); // フックはシーンのライティングの影響を受けない
    glColor3f(0.3f, 0.3f, 0.3f); // 金属フックの濃い灰色

    float hookHeight = 0.1f; // 短いフック
    float hookRadius = 0.02f; // 細いフック
    int segments = 16; // 円筒近似のセグメント数

    glPushMatrix();
    // 新しいランタン形状に合わせてランタンの上に配置
    glTranslatef(0.0f, 0.5f + hookHeight / 2.0f + 0.1f, 0.0f);

    // 上部の円を描画
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, hookHeight / 2.0f, 0.0f); // 上部円の中心
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    // 下部の円を描画
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, -hookHeight / 2.0f, 0.0f); // 下部円の中心
    for (int i = segments; i >= 0; --i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), -hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    // 円筒側面を描画
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), hookHeight / 2.0f, hookRadius * sin(angle));
        glVertex3f(hookRadius * cos(angle), -hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    glPopMatrix();

    glEnable(GL_LIGHTING); // ライティングを再有効化
}

// ランタンのフレームを描画する関数 (コムローイ風に更新 - 最小限の内部フレーム)
void drawLanternFrame() {
    glDisable(GL_LIGHTING); // フレームはシーンのライティングの影響を受けない
    glColor3f(0.4f, 0.2f, 0.0f); // 竹の濃い茶色 (非常に細い)

    float baseRadius = 0.35f; // 広い底
    float frameThickness = 0.015f; // 底のリング/フェンス用に少し厚く

    glPushMatrix();
    // ランタンの紙本体の最下部にリングを配置
    glTranslatef(0.0f, -0.6f, 0.0f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 30; ++i) { // より滑らかな円のためにセグメントを増やす
        float angle = 2.0f * M_PI * (float)i / 30.0f;
        glVertex3f(baseRadius * cos(angle), 0.0f, baseRadius * sin(angle));
        glVertex3f(baseRadius * cos(angle), frameThickness, baseRadius * sin(angle));
    }
    glEnd();
    glPopMatrix();

    // バーナーの土台を描画 (底のリングに接続された小さな四角)
    glColor3f(0.6f, 0.4f, 0.0f); // バーナー用に少し明るい茶色
    glPushMatrix();
    glTranslatef(0.0f, -0.6f - 0.1f, 0.0f); // 底のリングの下
    glScalef(baseRadius * 0.8f, frameThickness * 3.0f, baseRadius * 0.8f); // 平らな四角
    glutSolidCube(1.0);
    glPopMatrix();

    // ユーザーの要望により、他の垂直柱や上部リングはなし。

    glEnable(GL_LIGHTING); // ライティングを再有効化
}

// ランタンのカバーを描画する関数 (コムローイ風に更新 - 先細りの円筒)
void drawLanternCover() {
    glDisable(GL_LIGHTING); // カバーはシーンのライティングの影響を受けない

    // 完全に不透明になるようにアルファ値を調整
    glColor4f(1.0f, 0.9f, 0.7f, 1.0f); // 半透明の紙の色 (オフホワイト/クリーム色)、現在は完全に不透明

    float baseRadius = 0.34f; // フレームの底よりわずかに小さい
    // topRadiusは円筒形のためにbaseRadiusに非常に近い
    float topRadius = 0.33f;  // 非常にわずかなテーパー、ほぼ円筒
    float coverHeight = 1.2f; // 画像に合わせて高く
    int segments = 40; // より滑らかな曲線のためのセグメント数
    int stacks = 20;    // 垂直方向の滑らかさのためのスタック数

    // 主要な紙の本体のために先細りの円筒/円錐台を描画
    glPushMatrix();
    // カバーの底がフレームの底のリングと揃うように配置
    glTranslatef(0.0f, -coverHeight / 2.0f + (0.6f - coverHeight / 2.0f), 0.0f);

    for (int j = 0; j < stacks; ++j) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * M_PI * (float)i / (float)segments;

            float r1 = baseRadius + (topRadius - baseRadius) * (float)j / (float)stacks;
            float r2 = baseRadius + (topRadius - baseRadius) * (float)(j + 1) / (float)stacks;

            float y1 = (float)j / (float)stacks * coverHeight;
            float y2 = (float)(j + 1) / (float)stacks * coverHeight;

            // 現在のストリップセグメントの頂点
            glVertex3f(r1 * cos(angle), y1, r1 * sin(angle));
            glVertex3f(r2 * cos(angle), y2, r2 * sin(angle));
        }
        glEnd();
    }
    glPopMatrix();

    glEnable(GL_LIGHTING);  // ライティングを再有効化
}

// ランタンの屋根を描画する関数 (平らな屋根)
void drawLanternRoof() {
    glDisable(GL_LIGHTING); // 屋根はシーンのライティングの影響を受けない
    // ランタンカバーと同じ色を使用
    glColor4f(1.0f, 0.9f, 0.7f, 0.95f);

    float roofRadius = 0.35f; // ランタンカバーの上部よりわずかに大きい
    float roofThickness = 0.02f; // 平らな屋根のために非常に薄く
    int segments = 30; // 滑らかな円形のためのセグメント数

    glPushMatrix();
    // ランタンカバーの真上に屋根を配置
    // ランタンカバーの上部はY = 0.6f。したがって、屋根の底はY = 0.6f。
    glTranslatef(0.0f, 0.6f, 0.0f);

    // 平らな屋根の上面を描画 (円)
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, roofThickness, 0.0f); // 上面の中央
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), roofThickness, roofRadius * sin(angle));
    }
    glEnd();

    // 平らな屋根の底面を描画 (円)
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, 0.0f); // 底面の中央
    for (int i = segments; i >= 0; --i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), 0.0f, roofRadius * sin(angle));
    }
    glEnd();

    // 平らな屋根の側面を描画 (薄い円筒壁)
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), roofThickness, roofRadius * sin(angle));
        glVertex3f(roofRadius * cos(angle), 0.0f, roofRadius * sin(angle));
    }
    glEnd();

    glPopMatrix();
    glEnable(GL_LIGHTING); // ライティングを再有効化
}

// 個々のランタンを描画する関数 (KomLoyLanternオブジェクトを使用)
void drawSingleLantern(const KomLoyLantern& l) {
    glPushMatrix();
    glTranslatef(l.x, l.y, l.z); // ランタンのプロパティに基づいて位置を設定

    // ランタンの静的パーツのディスプレイリストを呼び出し
    glCallList(lanternDisplayList);

    // 炎はランタンごとに動的なので別途描画
    drawFlame(l.currentFlameAnimation, l.corePulsation);

    glPopMatrix();
}

// シンプルな地面を描画する関数
void drawGround() {
    glEnable(GL_LIGHTING); // 地面はシーンのライティングの影響を受ける
    glColor3f(0.05f, 0.05f, 0.1f); // 夜の地面用の非常に暗い青/黒

    glPushMatrix();
    // カメラの初期Y座標よりわずかに下に地面を配置し、XZ方向に広くする
    glTranslatef(0.0f, -0.5f, 0.0f); // カメラの視点レベルより下にY座標を調整

    glBegin(GL_QUADS);
    // ライティング用の法線を定義
    glNormal3f(0.0f, 1.0f, 0.0f); // 上向きの法線

    // 大きな四角形を描画
    float groundSize = 500.0f; // 非常に大きくする
    glVertex3f(-groundSize, 0.0f, -groundSize);
    glVertex3f(groundSize, 0.0f, -groundSize);
    glVertex3f(groundSize, 0.0f, groundSize);
    glVertex3f(-groundSize, 0.0f, groundSize);
    glEnd();
    glPopMatrix();
}

// 星を描画する関数
void drawStars() {
    glDisable(GL_LIGHTING); // 星は自己発光
    glDepthMask(GL_FALSE); // 星は「背景」なのでデプスバッファに書き込まない
    glPointSize(1.5f); // 星のサイズ

    glColor3f(0.8f, 0.8f, 1.0f); // 白から薄い青色の星

    glBegin(GL_POINTS);
    for (const auto& star : stars) {
        glVertex3f(star.x, star.y, star.z);
    }
    glEnd();

    glDepthMask(GL_TRUE); // デプス書き込みを再有効化
    glEnable(GL_LIGHTING); // ライティングを再有効化
}

// 初期化関数
void init() {
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f); // 夜空用の濃い青色の背景
    glEnable(GL_DEPTH_TEST); // 正しい3Dレンダリングのためのデプステストを有効化
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 500.0); // 遠くのランタンのために遠方クリッピング面を拡大
    glMatrixMode(GL_MODELVIEW);

    // 透明効果のためのブレンドを有効化
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 透明オブジェクトのデフォルトブレンド

    // ライティングを有効化
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0); // 光源0を有効化
    glEnable(GL_COLOR_MATERIAL); // glColor3fでマテリアルプロパティを設定できるようにする

    // 光源プロパティを定義 (シーン全体のための微妙なアンビエントライト)
    GLfloat light_position[] = { 0.0f, 100.0f, 0.0f, 0.0f }; // 上からの指向性ライト
    GLfloat light_ambient[] = { 0.1f, 0.1f, 0.15f, 1.0f }; // 非常に薄暗い青色のアンビエントライト
    GLfloat light_diffuse[] = { 0.3f, 0.3f, 0.4f, 1.0f }; // 薄暗い拡散光
    GLfloat light_specular[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // グローバル光源からのスペキュラライトなし

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // 炎のポリゴンをユニークなアニメーションオフセットで初期化
    for (int i = 0; i < 5; ++i) {
        flamePolygons.push_back({ 1.0f, 1.0f, 0.0f, 1.0f, getRandomFloat(0.0f, 100.0f) }); // カスタム乱数オフセットを使用
    }

    // 星を初期化
    for (int i = 0; i < NUM_STARS; ++i) {
        stars.push_back({ getRandomFloat(-200.0f, 200.0f), getRandomFloat(-200.0f, 200.0f), getRandomFloat(-200.0f, 200.0f) });
    }

    // --- ランタンの静的パーツ用のディスプレイリストを作成 ---
    lanternDisplayList = glGenLists(1); // ディスプレイリストの一意なIDを生成
    glNewList(lanternDisplayList, GL_COMPILE); // コマンドのリストへのコンパイルを開始
    // 単一のランタンの静的コンポーネントをローカル原点 (0,0,0) で描画
    drawLanternFrame();
    drawLanternCover();
    drawLanternRoof();
    drawHook();
    glEndList(); // ディスプレイリストのコンパイルを終了

    // ランタンを初期化
    for (int i = 0; i < NUM_LANTERNS; ++i) {
        KomLoyLantern newLantern;
        newLantern.x = getRandomFloat(-50.0f, 50.0f);
        newLantern.y = getRandomFloat(0.0f, 35.0f); // ランダムな初期Y座標に新しい分布を使用
        newLantern.z = getRandomFloat(-50.0f, 50.0f);
        newLantern.velX = getRandomFloat(-0.005f, 0.005f);
        newLantern.velY = getRandomFloat(0.01f, 0.03f);
        newLantern.velZ = getRandomFloat(-0.005f, 0.005f);
        newLantern.currentFlameAnimation = getRandomFloat(0.0f, 100.0f); // 各ランタンは独自の炎アニメーション時間を持つ
        newLantern.corePulsation = 0.0f; // フレームごとに計算される

        lanterns.push_back(newLantern);
    }
}

// ディスプレイコールバック関数
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // カラーバッファとデプスバッファをクリア
    glLoadIdentity(); // モデルビュー行列をリセット

    // --- 星を描画 (遠方効果のためにカメラ変換前) ---
    glPushMatrix();
    // 星にはカメラの回転のみを適用し、平行移動は適用しないことで、無限遠にあるように見せる
    glRotatef(-cameraAngleX, 1.0f, 0.0f, 0.0f); // ピッチ
    glRotatef(-cameraRotationY, 0.0f, 1.0f, 0.0f); // ヨー
    drawStars();
    glPopMatrix();

    // カメラ設定 (常に一人称視点)
    float eyeHeight = cameraY; // カメラの現在のY位置が視点の高さ
    float yawRad = cameraRotationY * M_PI / 180.0f;
    float pitchRad = cameraAngleX * M_PI / 180.0f; // cameraAngleXをピッチとして使用

    float lookDirX = cos(pitchRad) * sin(yawRad);
    float lookDirY = sin(pitchRad);
    float lookDirZ = -cos(pitchRad) * cos(yawRad); // OpenGLのフォワードは-Z方向

    gluLookAt(cameraX, eyeHeight, cameraZ, // カメラ位置
        cameraX + lookDirX, eyeHeight + lookDirY, cameraZ + lookDirZ, // 注視点
        0.0f, 1.0f, 0.0f);           // アップベクトル

    // 地面を描画
    drawGround();

    // 全てのランタンを描画
    for (const auto& l : lanterns) {
        drawSingleLantern(l);
    }

    glutSwapBuffers(); // フロントバッファとバックバッファをスワップ
}

// リシェイプコールバック関数
void reshape(int w, int h) {
    glViewport(0, 0, w, h); // 新しいウィンドウサイズにビューポートを設定
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 500.0); // パースペクティブ投影を更新
    glMatrixMode(GL_MODELVIEW);
}

// マウスコールバック関数
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        }
        else {
            mouseDragging = false;
        }
    }
}

// マウスモーションコールバック関数
void motion(int x, int y) {
    if (mouseDragging) {
        float deltaX = (float)(x - lastMouseX); // floatに明示的にキャスト
        float deltaY = (float)(y - lastMouseY); // floatに明示的にキャスト

        cameraRotationY += deltaX * 0.5f; // マウスXでカメラのヨーを回転

        // 上下方向の動きを統一 (マウスを上に動かせば視点も上を向くように)
        // cameraAngleX はピッチを表す。マウスを上に動かすと deltaY は負になる。
        // 視点を上に向けるには cameraAngleX の値を増やす必要がある。
        // よって、deltaY が負の時に cameraAngleX が増えるように、 -= を使う。
        // これで、マウスを上に動かすと視点も上に動く (Y軸反転なし)
        cameraAngleX -= deltaY * 0.5f;

        // カメラのピッチを制限して反転を防ぐ
        if (cameraAngleX > 60.0f) cameraAngleX = 60.0f; // 上限を60度に設定
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f; // 下限は-89度のまま

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay(); // 再描画を要求
    }
}

// キーボードキーダウンコールバック関数 (通常キー用)
void keyboard(unsigned char key, int x, int y) {
    keyStates[key] = true; // キーが押されたらキーの状態をtrueに設定
}

// キーボードキーアップコールバック関数 (通常キー用)
void keyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; // キーが離されたらキーの状態をfalseに設定
}

// 特殊キーボードキーダウンコールバック関数 (矢印キー用)
void specialKeyboard(int key, int x, int y) {
    specialKeyStates[key] = true; // 特殊キーが押されたら特殊キーの状態をtrueに設定
}

// 特殊キーボードキーアップコールバック関数 (矢印キー用)
void specialKeyboardUp(int key, int x, int y) {
    specialKeyStates[key] = false; // 特殊キーが離されたら特殊キーの状態をfalseに設定
}

// アニメーション更新のためのタイマー関数
void timer(int value) {
    float moveSpeed = 0.05f; // カメラの移動速度を増加

    // カメラのY軸回転 (向いている方向) に基づいて移動方向を計算
    float cameraFacingRad = cameraRotationY * M_PI / 180.0; // カメラ自身の回転を移動方向に使用

    float deltaMoveX = 0.0f;
    float deltaMoveZ = 0.0f;

    // 前方/後方移動
    if (specialKeyStates[GLUT_KEY_UP]) {
        deltaMoveX += sin(cameraFacingRad) * moveSpeed;
        deltaMoveZ -= cos(cameraFacingRad) * moveSpeed;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        deltaMoveX -= sin(cameraFacingRad) * moveSpeed;
        deltaMoveZ += cos(cameraFacingRad) * moveSpeed;
    }
    // 左右への平行移動 (カメラの向きに対して相対的)
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        // 左への平行移動は向いている方向から反時計回りに90度
        deltaMoveX -= cos(cameraFacingRad) * moveSpeed;
        deltaMoveZ -= sin(cameraFacingRad) * moveSpeed;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        // 右への平行移動は向いている方向から時計回りに90度
        deltaMoveX += cos(cameraFacingRad) * moveSpeed;
        deltaMoveZ += sin(cameraFacingRad) * moveSpeed;
    }

    cameraX += deltaMoveX;
    cameraZ += deltaMoveZ;

    // 定義されたカリングボックス (カメラの位置を基準)
    float cull_min_y = cameraY - 1.0f;
    float cull_max_y = cameraY + 37.5f;
    float cull_horizontal_radius = 50.0f;

    // 全てのランタンを更新
    for (auto& l : lanterns) {
        l.x += l.velX;
        l.y += l.velY;
        l.z += l.velZ;
        // 各ランタンの炎を個別にアニメーション
        l.currentFlameAnimation += 0.05f;
        l.corePulsation = (sin(l.currentFlameAnimation * 1.0f) + 1.0f) * 0.5f;

        // ランタンがカリングボックス外に出たらリセット
        if (l.y < cull_min_y || l.y > cull_max_y ||
            std::abs(l.x - cameraX) > cull_horizontal_radius ||
            std::abs(l.z - cameraZ) > cull_horizontal_radius)
        {
            // ランダムな位置に再出現 (カメラの現在位置を基準)
            l.y = cameraY + getRandomFloat(0.0f, 35.0f);
            l.x = cameraX + getRandomFloat(-50.0f, 50.0f);
            l.z = cameraZ + getRandomFloat(-50.0f, 50.0f);

            l.velX = getRandomFloat(-0.005f, 0.005f);
            l.velY = getRandomFloat(0.01f, 0.03f);
            l.velZ = getRandomFloat(-0.005f, 0.005f);
        }
    }

    glutPostRedisplay(); // 再描画を要求
    glutTimerFunc(16, timer, 0); // 約60 FPSでタイマーを再呼び出し
}

int main(int argc, char** argv) {
    glutInit(&argc, argv); // GLUTを初期化
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // ダブルバッファ、RGBカラー、デプスバッファ
    glutInitWindowSize(800, 600); // 初期ウィンドウサイズを設定
    glutCreateWindow("Kom Loy Festival Simulation"); // ウィンドウを作成

    init(); // カスタム初期化関数を呼び出し

    // コールバック関数を登録
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyboard);
    glutSpecialUpFunc(specialKeyboardUp);
    glutTimerFunc(0, timer, 0); // タイマーをすぐに開始

    glutMainLoop(); // GLUTイベント処理ループに入る
    return 0;
}
