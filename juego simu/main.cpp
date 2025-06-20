/**************************************
 * Escape the Grid – menú + carga mapa
 *  (SFML 2.x + tinyfiledialogs)
 *  ▸ BFS + ruta amarilla + FIX “un paso por pulsación”
 *************************************/
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

using namespace std;
using namespace sf;

// ---------- Constantes generales -------------
const float PI               = 3.14159265f;
const int   WINDOW_WIDTH     = 1200;
const int   WINDOW_HEIGHT    = 900;
const int   BATERIA_MAX      = 20;
const float CAMBIO_PROBABILIDAD = 0.05f;
const int   META_CAMBIO_CADA = 5;              // cada N movimientos

const float RADIO_BASE       = 43.f;
const float PASO_X_BASE      = 30.2671f;
const float PASO_Y_PAR_BASE  = 48.f;
const float PASO_Y_IMPAR_BASE= 40.f;

// ---------- Helpers gráficos -------------
FloatRect centerText(Text& t, float y)
{
    FloatRect b = t.getLocalBounds();
    t.setOrigin(b.left + b.width/2, b.top + b.height/2);
    t.setPosition(WINDOW_WIDTH/2.f, y);
    return t.getGlobalBounds();
}

enum class Estado { MENU_INICIO, MENU_CARGAR, JUEGO };

// ============================================================================
//  BFS – devuelve {f,c} desde inicio hasta meta (incluidos); vacío si no hay
// ============================================================================
vector<pair<int,int>> bfs(const vector<string>& mapa, int fIni, int cIni,
                          int fFin, int cFin)
{
    if (fIni == -1 || fFin == -1) return {};
    const int filas = mapa.size();
    map<pair<int,int>, pair<int,int>> parent;
    queue<pair<int,int>> q;
    q.push({fIni,cIni});
    parent[{fIni,cIni}] = {-1,-1};

    auto valido = [&](int f,int c)->bool{
        return f>=0 && f<filas &&
               c>=0 && c<(int)mapa[f].size() &&
               mapa[f][c]=='P';
    };

    while(!q.empty())
    {
        auto [f,c] = q.front(); q.pop();
        if (f==fFin && c==cFin) break;

        // vecinos según paridad
        vector<pair<int,int>> vecinos = {
            {f-1,c}, {f+1,c},
            (f%2==0)?pair<int,int>{f-1,c-1}:pair<int,int>{f+1,c-1},
            (f%2==0)?pair<int,int>{f-1,c+1}:pair<int,int>{f+1,c+1}
        };

        for(auto [nf,nc]:vecinos)
            if(valido(nf,nc) && !parent.count({nf,nc}))
            {
                parent[{nf,nc}] = {f,c};
                q.push({nf,nc});
            }
    }

    if(!parent.count({fFin,cFin})) return {};
    vector<pair<int,int>> ruta;
    for(pair<int,int> p={fFin,cFin}; p.first!=-1; p=parent[p])
        ruta.push_back(p);
    reverse(ruta.begin(), ruta.end());
    return ruta;
}

// *************************************************
//  FUNCIÓN DE JUEGO -------------------------------
// *************************************************
void ejecutarJuego(RenderWindow& window,
                   const string& archivoMapa,
                   Font& fuente)
{
    // ---------- Carga del mapa ----------
    ifstream archivo(archivoMapa);
    if (!archivo)
    {
        tinyfd_messageBox("Error",
            ("No pude abrir el archivo:\n" + archivoMapa).c_str(),
            "ok", "error", 1);
        return;
    }
    vector<string> mapaOriginal;
    string linea;
    while (getline(archivo, linea)) mapaOriginal.push_back(linea);
    vector<string> mapa = mapaOriginal;

    // ---------- Recursos ----------
    Texture bgTx;  bgTx.loadFromFile("espacio.png");
    Sprite fondo(bgTx);
    fondo.setScale(
        float(WINDOW_WIDTH) / bgTx.getSize().x,
        float(WINDOW_HEIGHT) / bgTx.getSize().y);

    static Texture txOn, txOff;
    static bool txCargadas = false;
    if (!txCargadas)
    {
        txOn .loadFromFile("PentagonosON.png");
        txOff.loadFromFile("PentagonosOFF.png");
        txCargadas = true;
    }

    // ---------- Geometría ----------
    size_t filas = mapa.size(), cols = 0;
    for (auto& l : mapa) if (l.size() > cols) cols = l.size();

    float escalaX = (WINDOW_WIDTH - 100.f) / (cols * PASO_X_BASE);
    float alturaTab = 0.f;
    for (size_t i = 0; i < filas; ++i)
        alturaTab += (i % 2 ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE);
    float escalaY = (WINDOW_HEIGHT - 150.f) / alturaTab;
    float escala  = min(escalaX, escalaY);

    float RADIO      = RADIO_BASE      * escala;
    float PASO_X     = PASO_X_BASE     * escala;
    float PASO_Y_PAR = PASO_Y_PAR_BASE * escala;
    float PASO_Y_IMP = PASO_Y_IMPAR_BASE*escala;

    map<pair<int,int>, Vector2f> posCelda;
    float yAcc = 70.f;
    for (size_t f = 0; f < filas; ++f)
    {
        for (size_t c = 0; c < mapa[f].size(); ++c)
        {
            float cx = c * PASO_X + 50.f;
            posCelda[{(int)f, (int)c}] = {cx, yAcc};
        }
        yAcc += (f % 2 ? PASO_Y_IMP : PASO_Y_PAR);
    }

    // ---------- Entidades ----------
    int fJug=-1, cJug=-1, fMeta=-1, cMeta=-1, movs=0, bateria=BATERIA_MAX;
    CircleShape jugador(RADIO/2), meta(RADIO/2);
    jugador.setFillColor(Color::Red);
    meta.setFillColor(Color::Green);
    jugador.setOrigin(RADIO/2,RADIO/2); meta.setOrigin(RADIO/2,RADIO/2);

    // barra batería
    RectangleShape barraFondo({200,20});
    barraFondo.setFillColor({50,50,50});
    barraFondo.setPosition(WINDOW_WIDTH-220,20);
    RectangleShape barraCarga; barraCarga.setPosition(WINDOW_WIDTH-220,20);
    Text txtBateria("Bateria: 20", fuente, 16);
    txtBateria.setPosition(WINDOW_WIDTH-215,45);

    // mensajes
    Clock relojMsg;
    Text msgMeta("", fuente, 24);
    msgMeta.setPosition(20,20);
    bool mostrarMsg=false;

    // ---------- BFS / AutoCamino ----------
    vector<pair<int,int>> ruta;       // camino actual
    bool mostrarRuta=false;           // dibujar en pantalla
    bool autoMover=false;             // activo desplazamiento auto
    size_t idxRuta=0;                 // posición en la ruta
    Clock relojPaso;                  // delay entre pasos

    auto recalcRuta = [&]()
    {
        ruta = bfs(mapa, fJug, cJug, fMeta, cMeta);
        mostrarRuta = !ruta.empty();
        idxRuta = 0;
    };

    auto celdaDisponible = [&](int f,int c)->bool{
        return f>=0 && f<(int)filas &&
               c>=0 && c<(int)mapa[f].size() &&
               mapa[f][c]=='P';
    };

    auto moverJugador = [&](int nf,int nc)
    {
        fJug = nf; cJug = nc;
        jugador.setPosition(posCelda[{fJug,cJug}]);
        bateria--; movs++; txtBateria.setString("Bateria: " + to_string(bateria));

        // ¿Meta alcanzada?
        if (fJug == fMeta && cJug == cMeta)
        { msgMeta.setString(u8"¡Meta alcanzada!"); mostrarMsg = true; relojMsg.restart(); autoMover=false; }

        // ¿Mover meta?
        if (movs % META_CAMBIO_CADA == 0)
        {
            vector<pair<int,int>> cand;
            for (size_t i = 0; i < filas; ++i)
                for (size_t j = 0; j < mapa[i].size(); ++j)
                    if (mapa[i][j] == 'P' && !(i == fJug && j == cJug))
                        cand.emplace_back(i,j);
            if (!cand.empty())
            {
                auto nm = cand[rand() % cand.size()];
                fMeta = nm.first; cMeta = nm.second;
                meta.setPosition(posCelda[{fMeta,cMeta}]);
            }
        }

        // Cambios aleatorios del tablero
        for (size_t i = 0; i < filas; ++i)
            for (size_t j = 0; j < mapa[i].size(); ++j)
            {
                if ((int)i == fJug && (int)j == cJug) continue;
                if ((int)i == fMeta && (int)j == cMeta) continue;
                if (mapaOriginal[i][j] == '.') continue;
                if (rand() % 100 < CAMBIO_PROBABILIDAD*100)
                    mapa[i][j] = (mapa[i][j] == 'P') ? '.' : 'P';
            }
    };

    // ---------- Bucle principal ----------
    while (window.isOpen())
    {
        Event ev;
        while (window.pollEvent(ev))
        {
            if (ev.type == Event::Closed) window.close();

            // Selección jugador / meta con ratón
            if (ev.type == Event::MouseButtonPressed &&
                ev.mouseButton.button == Mouse::Left)
            {
                Vector2i mp = Mouse::getPosition(window);
                for (auto& [coord, pos] : posCelda)
                {
                    float dist = hypot(pos.x - mp.x, pos.y - mp.y);
                    if (dist <= RADIO && mapa[coord.first][coord.second] == 'P')
                    {
                        if (fJug == -1)
                        { fJug = coord.first; cJug = coord.second; jugador.setPosition(pos); }
                        else if (fMeta == -1 && !(coord.first == fJug && coord.second == cJug))
                        { fMeta = coord.first; cMeta = coord.second; meta.setPosition(pos); }
                        break;
                    }
                }
            }

            // -------- MOVIMIENTO MANUAL (un paso por pulsación) -------------
            if (ev.type == Event::KeyPressed &&
                !autoMover && fJug!=-1 && fMeta!=-1 && bateria>0)
            {
                int nf=fJug, nc=cJug;
                bool mover=false;
                switch(ev.key.code)
                {
                    case Keyboard::W: nf--; mover=true; break;
                    case Keyboard::S: nf++; mover=true; break;
                    case Keyboard::A:
                        if (fJug%2==0){ nf--; nc--; } else { nf++; nc--; }
                        mover=true; break;
                    case Keyboard::D:
                        if (fJug%2==0){ nf--; nc++; } else { nf++; nc++; }
                        mover=true; break;
                    default: break;
                }
                if (mover && celdaDisponible(nf,nc))
                {
                    moverJugador(nf,nc);
                    mostrarRuta=false;      // ruta previa ya no es válida
                }
            }

            // Tecla R: calcular ruta y activar auto-movimiento
            if (ev.type == Event::KeyPressed && ev.key.code == Keyboard::R &&
                fJug!=-1 && fMeta!=-1)
            {
                recalcRuta();
                autoMover = mostrarRuta;
            }
        }

        // ---------- Movimiento automático por ruta ----------
        if (autoMover && bateria>0 && mostrarRuta && ruta.size()>1)
        {
            // Recalcular si alguna celda del camino cambió
            bool rutaValida=true;
            for(size_t i=idxRuta; i<ruta.size(); ++i)
                if(!celdaDisponible(ruta[i].first, ruta[i].second))
                { rutaValida=false; break; }
            if(!rutaValida)
            { recalcRuta(); if(!mostrarRuta) autoMover=false; }

            // Delay entre pasos
            if (autoMover && relojPaso.getElapsedTime().asMilliseconds() > 300 &&
                idxRuta+1 < ruta.size())
            {
                int nf = ruta[idxRuta+1].first;
                int nc = ruta[idxRuta+1].second;
                if(celdaDisponible(nf,nc))
                {
                    moverJugador(nf,nc);
                    idxRuta++;
                }
                relojPaso.restart();
            }
        }

        if (mostrarMsg && relojMsg.getElapsedTime().asSeconds() > 2) mostrarMsg=false;

        // ---------- Dibujado ----------
        window.clear();
        window.draw(fondo);

        // Pentágonos
        for (size_t f = 0; f < filas; ++f)
        {
            float rotDeg = (f % 2 ? 36.f : 0.f);
            for (size_t c = 0; c < mapa[f].size(); ++c)
            {
                if (mapaOriginal[f][c] == '.') continue;
                Sprite sp; sp.setTexture(mapa[f][c]=='P'?txOn:txOff);
                float factor = (RADIO*2) / sp.getTexture()->getSize().x;
                sp.setScale(factor, factor);
                sp.setOrigin(sp.getTexture()->getSize().x/2.f,
                             sp.getTexture()->getSize().y/2.f);
                sp.setPosition(posCelda[{(int)f,(int)c}]);
                sp.setRotation(rotDeg);
                window.draw(sp);
            }
        }

        // Línea amarilla de la ruta
        /*if (mostrarRuta && ruta.size()>1)
        {
            VertexArray strip(LinesStrip, ruta.size());
            for(size_t i=0;i<ruta.size();++i)
            {
                strip[i].position = posCelda[{ruta[i].first, ruta[i].second}];
                strip[i].color = Color::Yellow;
            }
            window.draw(strip);
        }*/
       // Puntos amarillos de la ruta
if (mostrarRuta && ruta.size()>1)
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


        if (fMeta!=-1) window.draw(meta);
        if (fJug !=-1) window.draw(jugador);

        // batería
        float wAct = 200.f * bateria / BATERIA_MAX;
        barraCarga.setSize({wAct, 20});
        float perc = float(bateria) / BATERIA_MAX;
        barraCarga.setFillColor( perc>0.66?Color::Green :
                                 perc>0.33?Color::Yellow : Color::Red );
        window.draw(barraFondo); window.draw(barraCarga); window.draw(txtBateria);

        if (mostrarMsg) window.draw(msgMeta);

        window.display();
    }
}

// ============================================================================
//  Menú principal
// ============================================================================
int main()
{
    srand((unsigned)time(nullptr));
    RenderWindow window(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
                        "Escape the Grid");
    window.setFramerateLimit(60);

    Font fuente;  fuente.loadFromFile("arial.ttf");

    Texture bgMenuTx; bgMenuTx.loadFromFile("labo.png");
    Sprite bgMenu(bgMenuTx);
    bgMenu.setScale(float(WINDOW_WIDTH) / bgMenuTx.getSize().x,
                    float(WINDOW_HEIGHT) / bgMenuTx.getSize().y);

    RectangleShape btnRect({250, 60});
    btnRect.setFillColor(Color(20,20,20,180));
    btnRect.setOrigin(btnRect.getSize().x/2.f, btnRect.getSize().y/2.f);

    Text btnTxt("Jugar", fuente, 28);
    btnTxt.setFillColor(Color::White);

    Estado estado = Estado::MENU_INICIO;
    string mapaSeleccionado;

    while (window.isOpen())
    {
        Event e;
        while (window.pollEvent(e))
        {
            if (e.type == Event::Closed) window.close();

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
                    const char* ruta =
                        tinyfd_openFileDialog("Selecciona nivel", "",
                                              1, filtros, "Nivel", 0);
                    if (ruta)
                    {
                        mapaSeleccionado = ruta;
                        estado = Estado::JUEGO;
                        ejecutarJuego(window, mapaSeleccionado, fuente);
                        estado = Estado::MENU_INICIO;
                    }
                }
            }
        }

        if (estado == Estado::MENU_INICIO || estado == Estado::MENU_CARGAR)
        {
            window.clear();
            window.draw(bgMenu);

            string textoBoton = (estado == Estado::MENU_INICIO) ? "Jugar" : "Cargar mapa";
            btnTxt.setString(textoBoton);
            FloatRect b = btnTxt.getLocalBounds();
            btnTxt.setOrigin(b.left + b.width/2.f, b.top + b.height/2.f);

            btnRect.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT*0.7f);
            btnTxt .setPosition(btnRect.getPosition());

            Text titulo("Escape the Grid", fuente, 48);
            titulo.setFillColor(Color::White);
            FloatRect tb = titulo.getLocalBounds();
            titulo.setOrigin(tb.left + tb.width/2.f, tb.top + tb.height/2.f);
            titulo.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT*0.25f);

            window.draw(btnRect); window.draw(btnTxt); window.draw(titulo);
            window.display();
        }
    }
    return 0;
}
