/*****************************************************
 *  Escape the Grid – versión FINAL protegida
 *  ▸ Jugador y meta nunca quedan aislados
 *  ▸ Cambios aleatorios reversibles
 *  ▸ Meta periódica siempre alcanzable
 *  ▸ Fin de juego con opciones
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

/* ------------ Constantes globales ------------ */
const float PI               = 3.14159265f;
const int   WINDOW_WIDTH     = 1200;
const int   WINDOW_HEIGHT    = 900;
const int   BATERIA_MAX      = 100;
const float CAMBIO_PROBABILIDAD = 0.05f;   // 5 % de prob. de mutar una celda
const int   META_CAMBIO_CADA = 10;         // N movimientos para mover meta

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
enum class Estado { MENU_INICIO, MENU_CARGAR, JUEGO };

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
            return true;
        }
    }

    return false;
}
/* =========================================================
   FUNCIÓN DE JUEGO: ciclo de ejecución principal del nivel
   =========================================================*/
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
    bgTx.loadFromFile("espacio.png");
    Sprite fondo(bgTx);
    fondo.setScale(float(WINDOW_WIDTH) / bgTx.getSize().x,
                   float(WINDOW_HEIGHT) / bgTx.getSize().y);

    static Texture txOn, txOff;
    static bool cargadas = false;
    if (!cargadas)
    {
        txOn.loadFromFile("PentagonosON.png");
        txOff.loadFromFile("PentagonosOFF.png");
        cargadas = true;
    }

    // ------ Geometría ------
    size_t filas = mapa.size(), cols = 0;
    for (auto& l : mapa) cols = max(cols, l.size());

    float escalaX = (WINDOW_WIDTH - 100.f) / (cols * PASO_X_BASE);
    float altura = 0.f;
    for (size_t i = 0; i < filas; ++i)
        altura += (i % 2 ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE);
    float escalaY = (WINDOW_HEIGHT - 150.f) / altura;
    float escala = min(escalaX, escalaY);

    float RADIO = RADIO_BASE * escala,
          PASO_X = PASO_X_BASE * escala,
          PASO_Y_PAR = PASO_Y_PAR_BASE * escala,
          PASO_Y_IMP = PASO_Y_IMPAR_BASE * escala;

    map<pair<int,int>, Vector2f> posCelda;
    float yAcc = 70.f;
    for (size_t f = 0; f < filas; ++f)
    {
        for (size_t c = 0; c < mapa[f].size(); ++c)
            posCelda[{(int)f, (int)c}] = {c * PASO_X + 50.f, yAcc};
        yAcc += (f % 2 ? PASO_Y_IMP : PASO_Y_PAR);
    }

    // ------ Jugador y meta ------
    int fJug = -1, cJug = -1, fMeta = -1, cMeta = -1;
    int movs = 0, bateria = BATERIA_MAX;
    CircleShape jugador(RADIO / 2), meta(RADIO / 2);
    jugador.setFillColor(Color::Red);
    meta.setFillColor(Color::Green);
    jugador.setOrigin(RADIO / 2, RADIO / 2);
    meta.setOrigin(RADIO / 2, RADIO / 2);

    RectangleShape barraFondo({200, 20});
    barraFondo.setFillColor({50, 50, 50});
    barraFondo.setPosition(WINDOW_WIDTH - 220, 20);

    RectangleShape barraCarga;
    barraCarga.setPosition(WINDOW_WIDTH - 220, 20);

    Text txtBateria("Bateria: 100", fuente, 16);
    txtBateria.setPosition(WINDOW_WIDTH - 215, 45);

    Clock relojMsg;
    Text msgMeta("", fuente, 24);
    msgMeta.setPosition(20, 20);
    bool mostrarMsg = false;

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
        bateria--; movs++;
        txtBateria.setString("Bateria: " + to_string(bateria));

        // 🏁 Victoria
        if (fJug == fMeta && cJug == cMeta)
        {
            autoMover = false;
            int resp = tinyfd_messageBox("¡Meta alcanzada!",
                "¡Felicitaciones!\n\n¿Deseas cargar otro mapa?\n(Sí = cargar, No = salir)",
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
                        }
                        else if (fMeta == -1 && !(coord.first == fJug && coord.second == cJug))
                        {
                            fMeta = coord.first; cMeta = coord.second;
                            meta.setPosition(pos);
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
        if (autoMover && bateria > 0 && mostrarRuta && ruta.size() > 1)
        {
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
        if (mostrarMsg) window.draw(msgMeta);

        window.display();
    }
    return volverMenuCarga;
}
int main()
{
    srand((unsigned)time(nullptr));
    RenderWindow window(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Escape the Grid");
    window.setFramerateLimit(60);

    Font fuente;
    if (!fuente.loadFromFile("arial.ttf"))
    {
        tinyfd_messageBox("Error", "No se pudo cargar la fuente arial.ttf", "ok", "error", 1);
        return 1;
    }

    Texture bgMenuTx;
    if (!bgMenuTx.loadFromFile("labo.png"))
    {
        tinyfd_messageBox("Error", "No se pudo cargar la imagen de fondo labo.png", "ok", "error", 1);
        return 1;
    }

    Sprite bgMenu(bgMenuTx);
    bgMenu.setScale(
        float(WINDOW_WIDTH) / bgMenuTx.getSize().x,
        float(WINDOW_HEIGHT) / bgMenuTx.getSize().y
    );

    RectangleShape btnRect({250, 60});
    btnRect.setFillColor(Color(20, 20, 20, 180));
    btnRect.setOrigin(btnRect.getSize().x / 2.f, btnRect.getSize().y / 2.f);

    Text btnTxt("Jugar", fuente, 28);
    btnTxt.setFillColor(Color::White);

    Estado estado = Estado::MENU_INICIO;
    string mapaSel;

    while (window.isOpen())
    {
        Event e;
        while (window.pollEvent(e))
        {
            if (e.type == Event::Closed)
                window.close();

            if (estado == Estado::MENU_INICIO &&
                e.type == Event::MouseButtonPressed &&
                e.mouseButton.button == Mouse::Left)
            {
                Vector2i mp = Mouse::getPosition(window);
                if (btnRect.getGlobalBounds().contains(float(mp.x), float(mp.y)))
                    estado = Estado::MENU_CARGAR;
            }
            else if (estado == Estado::MENU_CARGAR &&
                     e.type == Event::MouseButtonPressed &&
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

                        estado = volver ? Estado::MENU_CARGAR : Estado::MENU_INICIO;
                    }
                }
            }
        }

        if (!window.isOpen()) break;

        if (estado == Estado::MENU_INICIO || estado == Estado::MENU_CARGAR)
        {
            window.clear();
            window.draw(bgMenu);

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

            window.draw(btnRect);
            window.draw(btnTxt);
            window.draw(titulo);
            window.display();
        }
    }

    return 0;
}
