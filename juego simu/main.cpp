/*****************************************************
 *  Escape the Grid – versión FINAL configurable
 *  ▸ Pantalla de configuración interna (Batería y cambio de meta)
 *  ▸ Jugador y meta nunca quedan aislados
 *  ▸ Cambios aleatorios reversibles
 *  ▸ Meta periódica siempre alcanzable
 *  ▸ Fin de juego con opciones
 *  ▸ **NUEVO:** Jugador y meta son pentágonos (rojo y verde) con el
 *               mismo tamaño que las celdas del tablero
 *****************************************************/
#include <SFML/Graphics.hpp>
#include "tinyfiledialogs.h"
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <queue>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>

using namespace std;
using namespace sf;

/* ------------ Ajustes editables en tiempo de ejecución ------------ */
int   BATERIA_MAX      = 100;  // ← ahora variables globales
int   META_CAMBIO_CADA = 10;

/* ------------ Constantes globales (fijas) ------------ */
const float PI               = 3.14159265f;
const int   WINDOW_WIDTH     = 1200;
const int   WINDOW_HEIGHT    = 900;
const float CAMBIO_PROBABILIDAD = 0.05f;   // 5 % de prob. de mutar una celda

const float RADIO_BASE       = 43.f;
const float PASO_X_BASE      = 30.2671f;
const float PASO_Y_PAR_BASE  = 48.f;
const float PASO_Y_IMPAR_BASE= 40.f;

/* ------------ Utilidad para centrar texto ------------ */
FloatRect centerText(Text& t, float y)
{
    FloatRect b = t.getLocalBounds();
    t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    t.setPosition(WINDOW_WIDTH / 2.f, y);
    return t.getGlobalBounds();
}

/* ------------ Estado de la aplicación ------------ */
enum class Estado { MENU_INICIO, MENU_CONFIG, MENU_CARGAR, JUEGO };

/* =========================================================
   BFS – Ruta entre dos celdas  (vacía si no existe camino)
   =========================================================*/
vector<pair<int,int>> bfsRuta(const vector<string>& mapa,
                              int fIni,int cIni,int fFin,int cFin)
{
    if (fIni == -1 || fFin == -1) return {};
    const int filas = mapa.size();

    auto valido = [&](int f,int c)->bool {
        return f >= 0 && f < filas &&
               c >= 0 && c < (int)mapa[f].size() &&
               mapa[f][c] == 'P';
    };

    map<pair<int,int>, pair<int,int>> parent;
    queue<pair<int,int>> q;
    q.push({fIni,cIni});
    parent[{fIni,cIni}] = {-1,-1};

    while(!q.empty())
    {
        auto [f,c] = q.front(); q.pop();
        if (f == fFin && c == cFin) break;

        vector<pair<int,int>> vecinos = {
            {f-1, c}, {f+1, c},
            (f % 2 == 0) ? pair<int,int>{f-1, c-1} : pair<int,int>{f+1, c-1},
            (f % 2 == 0) ? pair<int,int>{f-1, c+1} : pair<int,int>{f+1, c+1}
        };

        for (auto [nf,nc] : vecinos)
            if (valido(nf,nc) && !parent.count({nf,nc}))
            {
                parent[{nf,nc}] = {f,c};
                q.push({nf,nc});
            }
    }

    if (!parent.count({fFin,cFin})) return {};
    vector<pair<int,int>> ruta;
    for (pair<int,int> p = {fFin,cFin}; p.first != -1; p = parent[p])
        ruta.push_back(p);
    reverse(ruta.begin(), ruta.end());
    return ruta;
}

/* =========================================================
   BFS – Área alcanzable completa desde (f0,c0)
   =========================================================*/
vector<pair<int,int>> bfsArea(const vector<string>& mapa,int f0,int c0)
{
    const int filas = mapa.size();
    auto valido = [&](int f,int c)->bool {
        return f >= 0 && f < filas &&
               c >= 0 && c < (int)mapa[f].size() &&
               mapa[f][c] == 'P';
    };

    vector<vector<char>> vis(filas);
    for (size_t i = 0; i < filas; ++i) vis[i].assign(mapa[i].size(), 0);

    queue<pair<int,int>> q;
    q.push({f0,c0}); vis[f0][c0] = 1;
    vector<pair<int,int>> out {{f0,c0}};

    while(!q.empty())
    {
        auto [f,c] = q.front(); q.pop();
        vector<pair<int,int>> vecinos = {
            {f-1, c}, {f+1, c},
            (f % 2 == 0) ? pair<int,int>{f-1, c-1} : pair<int,int>{f+1, c-1},
            (f % 2 == 0) ? pair<int,int>{f-1, c+1} : pair<int,int>{f+1, c+1}
        };
        for (auto [nf,nc] : vecinos)
            if (valido(nf,nc) && !vis[nf][nc])
            {
                vis[nf][nc] = 1;
                q.push({nf,nc});
                out.push_back({nf,nc});
            }
    }
    return out;
}
/* =========================================================
   Reconexión forzada: intenta abrir un camino hacia la meta
   =========================================================*/
bool forzarReconectar(vector<string>& mapa,int fJug,int cJug,int fMeta,int cMeta)
{
    auto areaJugador = bfsArea(mapa,fJug,cJug);

    for (auto& p : areaJugador)
        if (p.first == fMeta && p.second == cMeta)
            return true; // ya está conectado

    vector<pair<int,int>> fronteras;
    auto esValida = [&](int f,int c) {
        return f >= 0 && f < (int)mapa.size() &&
               c >= 0 && c < (int)mapa[f].size();
    };

    for (auto [f,c] : areaJugador)
    {
        vector<pair<int,int>> vecinos = {
            {f-1,c},{f+1,c},
            (f%2==0)?pair<int,int>{f-1,c-1}:pair<int,int>{f+1,c-1},
            (f%2==0)?pair<int,int>{f-1,c+1}:pair<int,int>{f+1,c+1}
        };
        for (auto [nf,nc] : vecinos)
            if (esValida(nf,nc) && mapa[nf][nc] == '.')
                fronteras.push_back({nf,nc});
    }

    for (auto [bf,bc] : fronteras)
    {
        mapa[bf][bc] = 'P'; // abrir temporalmente
        auto nuevaArea = bfsArea(mapa,fJug,cJug);
        for (auto& p : nuevaArea)
            if (p.first == fMeta && p.second == cMeta)
                return true;
        mapa[bf][bc] = '.'; // revertir
    }
    return false;
}

/* =========================================================
   Verifica si hay una ruta desde el jugador hacia una celda
   =========================================================*/
bool metaAlcanzable(const vector<string>& mapa, int fJug, int cJug, int fMeta, int cMeta)
{
    auto area = bfsArea(mapa, fJug, cJug);
    for (auto& p : area)
        if (p.first == fMeta && p.second == cMeta)
            return true;
    return false;
}

/* =========================================================
   Elige una nueva meta aleatoria que sea alcanzable
   =========================================================*/
bool elegirNuevaMeta(vector<string>& mapa, int fJug, int cJug,
                     int& fMeta, int& cMeta,
                     const map<pair<int,int>, Vector2f>& posCelda,
                     CircleShape& meta)
{
    vector<pair<int,int>> candidatos;
    for (size_t i = 0; i < mapa.size(); ++i)
        for (size_t j = 0; j < mapa[i].size(); ++j)
            if (mapa[i][j] == 'P' && !(i == fJug && j == cJug))
                candidatos.emplace_back(i, j);

    shuffle(candidatos.begin(), candidatos.end(), default_random_engine(time(nullptr)));

    for (auto [fi, ci] : candidatos)
    {
        if (metaAlcanzable(mapa, fJug, cJug, fi, ci))
        {
            fMeta = fi; cMeta = ci;
            meta.setPosition(posCelda.at({fMeta, cMeta}));
            meta.setRotation((fMeta % 2) ? 36.f : 0.f); // ← orientación correcta
            return true;
        }
    }

    return false;
}
/* =========================================================
   FUNCIÓN DE JUEGO: ciclo de ejecución principal del nivel
   =========================================================*/
   float margenLateralIzq = 50.f;
float anchoPanelDerecho = 250.f;  // ← espacio reservado para batería y controles

bool ejecutarJuego(RenderWindow& window,
                   const string& archivoMapa,
                   Font& fuente)
{
    ifstream archivo(archivoMapa);
    if (!archivo)
    {
        tinyfd_messageBox("Error",
                          ("No se pudo abrir:\n" + archivoMapa).c_str(),
                          "ok", "error", 1);
        return false;
    }

    vector<string> mapaOriginal;
    string linea;
    while (getline(archivo, linea)) mapaOriginal.push_back(linea);
    vector<string> mapa = mapaOriginal;

    // ------ Texturas y fondo ------
    Texture bgTx;
    bgTx.loadFromFile("imgytext/espacio.png");
    Sprite fondo(bgTx);
    fondo.setScale(float(WINDOW_WIDTH) / bgTx.getSize().x,
                   float(WINDOW_HEIGHT) / bgTx.getSize().y);

    static Texture txOn, txOff;
    static bool cargadas = false;
    if (!cargadas)
    {
        txOn.loadFromFile("imgytext/PentagonosON.png");
        txOff.loadFromFile("imgytext/PentagonosOFF.png");
        cargadas = true;
    }

    // ------ Geometría ------
    size_t filas = mapa.size(), cols = 0;
    for (auto& l : mapa) cols = max(cols, l.size());
/*
    float escalaX = (WINDOW_WIDTH - 100.f) / (cols * PASO_X_BASE);
    float altura = 0.f;
    for (size_t i = 0; i < filas; ++i)
        altura += (i % 2 ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE);
    float escalaY = (WINDOW_HEIGHT - 150.f) / altura;
    float escala = min(escalaX, escalaY);*/

    // Márgenes para que la barra y el texto no se tapen
float margenSuperior = 80.f;
float margenInferior = 30.f;
float margenLateral = 80.f;

//float escalaX = (WINDOW_WIDTH - margenLateral * 2.f) / (cols * PASO_X_BASE);
float escalaX = (WINDOW_WIDTH - anchoPanelDerecho - margenLateralIzq) / (cols * PASO_X_BASE);


float altura = 0.f;
for (size_t i = 0; i < filas; ++i)
    altura += (i % 2 ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE);

float escalaY = (WINDOW_HEIGHT - margenSuperior - margenInferior) / altura;
float escala = min(escalaX, escalaY);


    float RADIO = RADIO_BASE * escala,
          PASO_X = PASO_X_BASE * escala,
          PASO_Y_PAR = PASO_Y_PAR_BASE * escala,
          PASO_Y_IMP = PASO_Y_IMPAR_BASE * escala;

    map<pair<int,int>, Vector2f> posCelda;
    //float yAcc = 70.f;
    float yAcc = margenSuperior;
    for (size_t f = 0; f < filas; ++f)
    {
        for (size_t c = 0; c < mapa[f].size(); ++c)
            posCelda[{(int)f, (int)c}] = {c * PASO_X + 50.f, yAcc};
        yAcc += (f % 2 ? PASO_Y_IMP : PASO_Y_PAR);
    }

    // ------ Jugador y meta ------
    int fJug = -1, cJug = -1, fMeta = -1, cMeta = -1;
    int fMetaAnt = -1, cMetaAnt = -1;
    int movs = 0, bateria = BATERIA_MAX;
    Clock relojJuego;  // ← mide el tiempo total del nivel


    // ▸ Cargar texturas para jugador y meta
    Texture npcTexture, metaTexture;
    npcTexture.loadFromFile("imgytext/npc.png");
    metaTexture.loadFromFile("imgytext/meta.png");

    // ▸▸ PENTÁGONOS (5 lados) en vez de círculos
    CircleShape jugador(RADIO, 5), meta(RADIO, 5);
    jugador.setTexture(&npcTexture);
    meta.setTexture(&metaTexture);
    jugador.setScale(0.9f, 0.9f);
    meta.setScale(0.9f, 0.9f);
    jugador.setOrigin(RADIO, RADIO);
    meta.setOrigin(RADIO, RADIO);

    RectangleShape barraFondo({200, 20});
    barraFondo.setFillColor({50, 50, 50});
    //barraFondo.setPosition(WINDOW_WIDTH - 220, 20);

    RectangleShape barraCarga;
    //barraCarga.setPosition(WINDOW_WIDTH - 220, 20);

    Text txtBateria("Bateria: " + to_string(BATERIA_MAX), fuente, 16);
    //txtBateria.setPosition(WINDOW_WIDTH - 215, 45);

    float xPanel = WINDOW_WIDTH - anchoPanelDerecho + 20.f;
barraFondo.setPosition(xPanel, 30);
barraCarga.setPosition(xPanel, 30);
txtBateria.setPosition(xPanel + 5, 55);


    Clock relojMsg;
    Text msgMeta("", fuente, 24);
    msgMeta.setPosition(20, 20);
    bool mostrarMsg = false;

    Text txtControles("Controles:\nW/S: Arriba/Abajo\nA/D: izquierda/derecha\nClick: colocar jugadorYmeta\nR: RutaAutomatica\n", fuente, 16);
txtControles.setFillColor(Color::White);
txtControles.setPosition(xPanel, 100);


    vector<pair<int,int>> ruta;
    bool mostrarRuta = false, autoMover = false;
    size_t idxRuta = 0;
    Clock relojPaso;

    auto actualizarRuta = [&]() {
        ruta = bfsRuta(mapa, fJug, cJug, fMeta, cMeta);
        mostrarRuta = !ruta.empty();
        idxRuta = 0;
    };

    auto celdaOk = [&](int f, int c) -> bool {
        return f >= 0 && f < (int)filas &&
               c >= 0 && c < (int)mapa[f].size() &&
               mapa[f][c] == 'P';
    };
    bool volverMenuCarga = false;

    auto aplicarCambiosAleatorios = [&]() {
        for (size_t i = 0; i < filas; ++i)
            for (size_t j = 0; j < mapa[i].size(); ++j)
            {
                if ((int)i == fJug && (int)j == cJug) continue;
                if ((int)i == fMeta && (int)j == cMeta) continue;
                if (mapaOriginal[i][j] == '.') continue;
                if (rand() % 100 < CAMBIO_PROBABILIDAD * 100)
                    mapa[i][j] = (mapa[i][j] == 'P') ? '.' : 'P';
            }
    };

    auto moverJugador = [&](int nf, int nc) {
        fJug = nf; cJug = nc;
        jugador.setPosition(posCelda[{fJug, cJug}]);
        jugador.setRotation((fJug % 2) ? 36.f : 0.f);
        bateria--; movs++;
        txtBateria.setString("Bateria: " + to_string(bateria));

        // 🏁 Victoria
        /*if (fJug == fMeta && cJug == cMeta)
        {
            autoMover = false;
            int resp = tinyfd_messageBox("¡Meta alcanzada!",
                "¡Felicitaciones!\n\n¿Deseas cargar otro mapa?\n(Sí = cargar, No = salir)",
                "yesno", "info", 1);*/

                if (fJug == fMeta && cJug == cMeta)
{
    autoMover = false;

    float segundos = relojJuego.getElapsedTime().asSeconds();
    string msg = "¡Felicitaciones!\n\n"
                 "Movimientos: " + to_string(movs) +
                 "\nTiempo: " + to_string((int)segundos) + " segundos\n\n"
                 "¿Deseas cargar otro mapa?\n(Sí = cargar, No = salir)";

    int resp = tinyfd_messageBox("¡Meta alcanzada!",
                                 msg.c_str(),
                                 "yesno", "info", 1);

            if (resp == 1)   volverMenuCarga = true;
            else             window.close();
            return;
        }

        // 🔁 Cambio de meta periódico
        if (movs % META_CAMBIO_CADA == 0)
        {
            if (!elegirNuevaMeta(mapa, fJug, cJug, fMeta, cMeta, posCelda, meta))
            {
                tinyfd_messageBox("Alerta",
                    "No se pudo colocar una nueva meta alcanzable.",
                    "ok", "warning", 1);
                window.close(); return;
            }
        }

        // 🔀 Mutaciones aleatorias del mapa
        vector<string> respaldo = mapa;
        aplicarCambiosAleatorios();

        if (!metaAlcanzable(mapa, fJug, cJug, fMeta, cMeta))
        {
            bool ok = forzarReconectar(mapa, fJug, cJug, fMeta, cMeta);
            if (!ok) mapa = respaldo;
        }
    };
    // BUCLE PRINCIPAL
    while (window.isOpen() && !volverMenuCarga)
    {
        Event ev;
        while (window.pollEvent(ev))
        {
            if (ev.type == Event::Closed) window.close();

            // Click: seleccionar jugador o meta
            if (ev.type == Event::MouseButtonPressed && ev.mouseButton.button == Mouse::Left)
            {
                Vector2i mp = Mouse::getPosition(window);
                for (auto& [coord, pos] : posCelda)
                {
                    float d = hypot(pos.x - mp.x, pos.y - mp.y);
                    if (d <= RADIO && mapa[coord.first][coord.second] == 'P')
                    {
                        if (fJug == -1)
                        {
                            fJug = coord.first; cJug = coord.second;
                            jugador.setPosition(pos);
                            jugador.setRotation((fJug % 2) ? 36.f : 0.f);
                        }
                        else if (fMeta == -1 && !(coord.first == fJug && coord.second == cJug))
                        {
                            fMeta = coord.first; cMeta = coord.second;
                            meta.setPosition(pos);
                            meta.setRotation((fMeta % 2) ? 36.f : 0.f);
                        }
                        break;
                    }
                }
            }

            // Movimiento manual
            if (ev.type == Event::KeyPressed && !autoMover &&
                fJug != -1 && fMeta != -1 && bateria > 0)
            {
                int nf = fJug, nc = cJug; bool mover = false;
                switch (ev.key.code)
                {
                    case Keyboard::W: nf--; mover = true; break;
                    case Keyboard::S: nf++; mover = true; break;
                    case Keyboard::A:
                        if (fJug % 2 == 0) { nf--; nc--; } else { nf++; nc--; }
                        mover = true; break;
                    case Keyboard::D:
                        if (fJug % 2 == 0) { nf--; nc++; } else { nf++; nc++; }
                        mover = true; break;
                    default: break;
                }
                if (mover && celdaOk(nf, nc)) {
                    moverJugador(nf, nc); mostrarRuta = false;
                }
            }

            // Tecla R: mostrar ruta y activar movimiento automático
            if (ev.type == Event::KeyPressed && ev.key.code == Keyboard::R &&
                fJug != -1 && fMeta != -1)
            {
                actualizarRuta();
                autoMover = mostrarRuta;
            }
        }

        // Movimiento automático paso a paso
        if (autoMover && bateria > 0 && ruta.size() > 1)
        {
            // 🔄 Recalcular ruta si la meta cambió
            if (fMeta != fMetaAnt || cMeta != cMetaAnt) {
                actualizarRuta();
                fMetaAnt = fMeta;
                cMetaAnt = cMeta;
            }

            bool rutaOk = true;
            for (size_t i = idxRuta; i < ruta.size(); ++i)
                if (!celdaOk(ruta[i].first, ruta[i].second))
                    { rutaOk = false; break; }

            if (!rutaOk)
            {
                actualizarRuta();
                if (!mostrarRuta) autoMover = false;
            }

            if (autoMover &&
                relojPaso.getElapsedTime().asMilliseconds() > 300 &&
                idxRuta + 1 < ruta.size())
            {
                int nf = ruta[idxRuta + 1].first,
                    nc = ruta[idxRuta + 1].second;
                if (celdaOk(nf, nc)) { moverJugador(nf, nc); idxRuta++; }
                relojPaso.restart();
            }
        }

        if (mostrarMsg && relojMsg.getElapsedTime().asSeconds() > 2)
            mostrarMsg = false;

        // ================== RENDER ==================
        window.clear(); window.draw(fondo);
        // PANEL DERECHO DE INFORMACIÓN
RectangleShape panelInfo;
panelInfo.setSize(Vector2f(anchoPanelDerecho, WINDOW_HEIGHT));
panelInfo.setFillColor(Color(0, 0, 0, 160));
panelInfo.setPosition(WINDOW_WIDTH - anchoPanelDerecho, 0);
window.draw(panelInfo);


        for (size_t f = 0; f < filas; ++f)
        {
            float rot = f % 2 ? 36.f : 0.f;
            for (size_t c = 0; c < mapa[f].size(); ++c)
            {
                if (mapaOriginal[f][c] == '.') continue;
                Sprite sp; sp.setTexture(mapa[f][c] == 'P' ? txOn : txOff);
                float factor = (RADIO * 2) / sp.getTexture()->getSize().x;
                sp.setScale(factor, factor);
                sp.setOrigin(sp.getTexture()->getSize().x / 2.f,
                             sp.getTexture()->getSize().y / 2.f);
                sp.setPosition(posCelda[{(int)f, (int)c}]);
                sp.setRotation(rot);
                window.draw(sp);
            }
        }

        if (mostrarRuta && ruta.size() > 1)
        {
            CircleShape punto(RADIO / 4.f);
            punto.setFillColor(Color::Yellow);
            punto.setOrigin(punto.getRadius(), punto.getRadius());
            for (auto& [f, c] : ruta)
            {
                punto.setPosition(posCelda[{f, c}]);
                window.draw(punto);
            }
        }

        if (fMeta != -1) window.draw(meta);
        if (fJug  != -1) window.draw(jugador);

        // Dibujar batería
        float w = 200.f * bateria / BATERIA_MAX;
        barraCarga.setSize({w, 20});
        float perc = float(bateria) / BATERIA_MAX;
        barraCarga.setFillColor( perc > 0.66 ? Color::Green :
                                 perc > 0.33 ? Color::Yellow : Color::Red );
        window.draw(barraFondo); window.draw(barraCarga);
        window.draw(txtBateria);
        window.draw(txtControles);

        if (mostrarMsg) window.draw(msgMeta);

        window.display();
    }
    return volverMenuCarga;
}
/* =========================================================
   ========================  MAIN  =========================
   =========================================================*/
int main()
{
    srand((unsigned)time(nullptr));
    RenderWindow window(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Escape the Grid");
    window.setFramerateLimit(60);

    Font fuente;
    if (!fuente.loadFromFile("imgytext/arial.ttf"))
    {
        tinyfd_messageBox("Error", "No se pudo cargar la fuente arial.ttf", "ok", "error", 1);
        return 1;
    }

    Texture bgMenuTx;
    if (!bgMenuTx.loadFromFile("imgytext/labo.png"))
    {
        tinyfd_messageBox("Error", "No se pudo cargar la imagen de fondo labo.png", "ok", "error", 1);
        return 1;
    }
    Sprite bgMenu(bgMenuTx);
    bgMenu.setScale(float(WINDOW_WIDTH) / bgMenuTx.getSize().x,
                    float(WINDOW_HEIGHT) / bgMenuTx.getSize().y);

    /* ---------- Elementos comunes de UI ---------- */
    RectangleShape btnRect({250, 60});
    btnRect.setFillColor(Color(20, 20, 20, 180));
    btnRect.setOrigin(btnRect.getSize().x / 2.f, btnRect.getSize().y / 2.f);

    Text btnTxt("Jugar", fuente, 28);
    btnTxt.setFillColor(Color::White);

    /* ---------- Elementos de la PANTALLA CONFIG ---------- */
    RectangleShape campoBat({300, 50});
    campoBat.setFillColor(Color(20,20,20,180));
    campoBat.setOrigin(campoBat.getSize().x/2.f, campoBat.getSize().y/2.f);
    RectangleShape campoMeta(campoBat);

    Text lblBat("Bateria maxima:", fuente, 24);
    Text lblMeta("Meta cambia cada:", fuente, 24);
    Text txtBat(to_string(BATERIA_MAX), fuente, 24);
    Text txtMeta(to_string(META_CAMBIO_CADA), fuente, 24);

    RectangleShape btnNext({200, 50});
    btnNext.setFillColor(Color(20,20,20,180));
    btnNext.setOrigin(btnNext.getSize().x/2.f, btnNext.getSize().y/2.f);
    Text txtNext("Siguiente", fuente, 24);
    txtNext.setFillColor(Color::White);

    string batStr = to_string(BATERIA_MAX);
    string metaStr = to_string(META_CAMBIO_CADA);
    bool editBat = false, editMeta = false;

    /* ---------- Estado inicial ---------- */
    Estado estado = Estado::MENU_INICIO;
    string mapaSel;

    while (window.isOpen())
    {
        Event e;
        while (window.pollEvent(e))
        {
            if (e.type == Event::Closed)
                window.close();

            /* ---------------- MENU INICIO ---------------- */
            if (estado == Estado::MENU_INICIO)
            {
                if (e.type == Event::MouseButtonPressed &&
                    e.mouseButton.button == Mouse::Left)
                {
                    Vector2i mp = Mouse::getPosition(window);
                    if (btnRect.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                        estado = Estado::MENU_CONFIG;
                }
            }
            /* ---------------- MENU CONFIG ---------------- */
            else if (estado == Estado::MENU_CONFIG)
            {
                if (e.type == Event::MouseButtonPressed &&
                    e.mouseButton.button == Mouse::Left)
                {
                    Vector2i mp = Mouse::getPosition(window);
                    if (campoBat.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                        { editBat = true; editMeta = false; }
                    else if (campoMeta.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                        { editMeta = true; editBat = false; }
                    else if (btnNext.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                    {
                        /* Validar números */
                        try {
                            int nb = stoi(batStr);
                            int nm = stoi(metaStr);
                            if (nb > 0 && nm > 0)
                            { BATERIA_MAX = nb; META_CAMBIO_CADA = nm; }
                        } catch (...) { /* si error, se quedan valores anteriores */ }

                        editBat = editMeta = false;
                        estado = Estado::MENU_CARGAR;
                    }
                    else { editBat = editMeta = false; }
                }
                /* Capturar texto */
                if (e.type == Event::TextEntered)
                {
                    if (editBat)
                    {
                        if (e.text.unicode == 8) { if (!batStr.empty()) batStr.pop_back(); }
                        else if (isdigit(e.text.unicode) && batStr.size()<4)
                            batStr.push_back(char(e.text.unicode));
                        txtBat.setString(batStr.empty()?"0":batStr);
                    }
                    else if (editMeta)
                    {
                        if (e.text.unicode == 8) { if (!metaStr.empty()) metaStr.pop_back(); }
                        else if (isdigit(e.text.unicode) && metaStr.size()<4)
                            metaStr.push_back(char(e.text.unicode));
                        txtMeta.setString(metaStr.empty()?"0":metaStr);
                    }
                }
            }
            /* ---------------- MENU CARGAR ---------------- */
            else if (estado == Estado::MENU_CARGAR)
            {
                if (e.type == Event::MouseButtonPressed &&
                    e.mouseButton.button == Mouse::Left)
                {
                    Vector2i mp = Mouse::getPosition(window);
                    if (btnRect.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                    {
                        const char* filtros[1] = { "*.txt" };
                        const char* ruta = tinyfd_openFileDialog("Selecciona nivel", "", 1,
                                                                 filtros, "Nivel", 0);
                        if (ruta)
                        {
                            mapaSel = ruta;
                            estado = Estado::JUEGO;

                            bool volver = ejecutarJuego(window, mapaSel, fuente);
                            if (!window.isOpen()) break;
                            estado = volver ? Estado::MENU_CONFIG : Estado::MENU_INICIO;
                        }
                    }
                }
            }
        }

        if (!window.isOpen()) break;

        /* --------------- RENDER de los diferentes menús --------------- */
        if (estado == Estado::MENU_INICIO || estado == Estado::MENU_CARGAR)
        {
            window.clear(); window.draw(bgMenu);

            string textoBoton = (estado == Estado::MENU_INICIO) ? "Jugar" : "Cargar mapa";
            btnTxt.setString(textoBoton);
            FloatRect b = btnTxt.getLocalBounds();
            btnTxt.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);

            btnRect.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT * 0.7f);
            btnTxt.setPosition(btnRect.getPosition());

            Text titulo("Escape the Grid", fuente, 48);
            titulo.setFillColor(Color::White);
            FloatRect tb = titulo.getLocalBounds();
            titulo.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
            titulo.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT * 0.25f);

            window.draw(btnRect); window.draw(btnTxt); window.draw(titulo);
            window.display();
        }
        else if (estado == Estado::MENU_CONFIG)
        {
            window.clear(); window.draw(bgMenu);

            /* Posicionar elementos */
            campoBat.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT*0.45f);
            campoMeta.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT*0.60f);

            lblBat.setPosition(campoBat.getPosition().x - campoBat.getSize().x/2.f,
                               campoBat.getPosition().y - 40);
            lblMeta.setPosition(campoMeta.getPosition().x - campoMeta.getSize().x/2.f,
                                campoMeta.getPosition().y - 40);

            txtBat.setPosition(campoBat.getPosition().x - txtBat.getLocalBounds().width/2.f,
                               campoBat.getPosition().y - txtBat.getLocalBounds().height/2.f);
            txtMeta.setPosition(campoMeta.getPosition().x - txtMeta.getLocalBounds().width/2.f,
                                campoMeta.getPosition().y - txtMeta.getLocalBounds().height/2.f);

            /* Resaltar campo activo */
            campoBat.setOutlineThickness(editBat?3.f:0.f);
            campoBat.setOutlineColor(Color::Yellow);
            campoMeta.setOutlineThickness(editMeta?3.f:0.f);
            campoMeta.setOutlineColor(Color::Yellow);

            /* Botón siguiente */
            btnNext.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT*0.8f);
            txtNext.setPosition(btnNext.getPosition().x - txtNext.getLocalBounds().width/2.f,
                                btnNext.getPosition().y - txtNext.getLocalBounds().height/2.f);

            /* Dibujar */
            window.draw(campoBat);  window.draw(campoMeta);
            window.draw(lblBat);    window.draw(lblMeta);
            window.draw(txtBat);    window.draw(txtMeta);
            window.draw(btnNext);   window.draw(txtNext);
            window.display();
        }
    }

    return 0;
}
