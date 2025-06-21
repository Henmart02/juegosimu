/**************************************
 * Escape the Grid
 *  (SFML 2.x + tinyfiledialogs)
 *  ▸ BFS + ruta amarilla + FIX “un paso”
 *  ▸ Protección anti-encierro: reconexión forzada
 *  ▸ FIN DE JUEGO: “Meta alcanzada” con 2 opciones
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
const int   BATERIA_MAX      = 100;
const float CAMBIO_PROBABILIDAD = 0.05f;
const int   META_CAMBIO_CADA = 10;              // cada N movimientos

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
//  BFS – ruta entre dos puntos  (devuelve vacía si no hay)
// ============================================================================
vector<pair<int,int>> bfsRuta(const vector<string>& mapa,
                              int fIni,int cIni,int fFin,int cFin)
{
    if (fIni==-1||fFin==-1) return {};
    const int filas = mapa.size();
    map<pair<int,int>, pair<int,int>> parent;
    queue<pair<int,int>> q;
    auto valido=[&](int f,int c)->bool{
        return f>=0 && f<filas &&
               c>=0 && c<(int)mapa[f].size() &&
               mapa[f][c]=='P';
    };

    q.push({fIni,cIni});
    parent[{fIni,cIni}]={-1,-1};

    while(!q.empty())
    {
        auto [f,c]=q.front(); q.pop();
        if(f==fFin&&c==cFin) break;

        vector<pair<int,int>> vec={
            {f-1,c},{f+1,c},
            (f%2==0)?pair<int,int>{f-1,c-1}:pair<int,int>{f+1,c-1},
            (f%2==0)?pair<int,int>{f-1,c+1}:pair<int,int>{f+1,c+1}
        };

        for(auto [nf,nc]:vec)
            if(valido(nf,nc)&&!parent.count({nf,nc}))
            { parent[{nf,nc}]={f,c}; q.push({nf,nc}); }
    }

    if(!parent.count({fFin,cFin})) return {};
    vector<pair<int,int>> ruta;
    for(pair<int,int> p={fFin,cFin};p.first!=-1;p=parent[p])
        ruta.push_back(p);
    reverse(ruta.begin(), ruta.end());
    return ruta;
}

// ============================================================================
//  BFS – devuelve TODAS las celdas alcanzables desde (f0,c0)
// ============================================================================
vector<pair<int,int>> bfsArea(const vector<string>& mapa,int f0,int c0)
{
    const int filas=mapa.size();
    auto valido=[&](int f,int c)->bool{
        return f>=0&&f<filas&&
               c>=0&&c<(int)mapa[f].size()&&
               mapa[f][c]=='P';
    };
    vector<vector<char>> vis(filas);
    for(size_t i=0;i<filas;++i) vis[i].assign(mapa[i].size(),0);

    queue<pair<int,int>> q;
    q.push({f0,c0}); vis[f0][c0]=1;
    vector<pair<int,int>> out{{f0,c0}};

    while(!q.empty())
    {
        auto [f,c]=q.front(); q.pop();
        vector<pair<int,int>> vec={
            {f-1,c},{f+1,c},
            (f%2==0)?pair<int,int>{f-1,c-1}:pair<int,int>{f+1,c-1},
            (f%2==0)?pair<int,int>{f-1,c+1}:pair<int,int>{f+1,c+1}
        };
        for(auto [nf,nc]:vec)
            if(valido(nf,nc)&&!vis[nf][nc])
            { vis[nf][nc]=1; q.push({nf,nc}); out.push_back({nf,nc}); }
    }
    return out;
}

// ============================================================================
//  Reconexión forzada: intenta desbloquear una celda vecina para enlazar
//  la zona del jugador con la meta. Devuelve true si lo logró.
// ============================================================================
bool forzarReconectar(vector<string>& mapa,int fJug,int cJug,int fMeta,int cMeta)
{
    auto areaJugador=bfsArea(mapa,fJug,cJug);
    // Si la meta ya es alcanzable, no hace falta
    for(auto& p:areaJugador)
        if(p.first==fMeta&&p.second==cMeta) return true;

    // 1️⃣ recolectar fronteras de la isla del jugador
    vector<pair<int,int>> fronteras;
    auto esValida=[&](int f,int c)->bool{
        return f>=0&&f<(int)mapa.size()&&
               c>=0&&c<(int)mapa[f].size();
    };
    for(auto [f,c]:areaJugador)
    {
        vector<pair<int,int>> vec={
            {f-1,c},{f+1,c},
            (f%2==0)?pair<int,int>{f-1,c-1}:pair<int,int>{f+1,c-1},
            (f%2==0)?pair<int,int>{f-1,c+1}:pair<int,int>{f+1,c+1}
        };
        for(auto [nf,nc]:vec)
            if(esValida(nf,nc)&&mapa[nf][nc]=='.')
                fronteras.push_back({nf,nc});
    }
    // 2️⃣ probar desbloquear cada frontera y ver si conecta
    for(auto [bf,bc]:fronteras)
    {
        mapa[bf][bc]='P';                 // desbloqueo temporal
        auto nuevaArea=bfsArea(mapa,fJug,cJug);
        bool metaDentro=false;
        for(auto &p:nuevaArea)
            if(p.first==fMeta&&p.second==cMeta)
            { metaDentro=true; break; }
        if(metaDentro) return true;       // ¡Conexión lograda!
        mapa[bf][bc]='.';                 // revertir y probar siguiente
    }
    return false; // no se pudo
}

// *************************************************
//  FUNCIÓN DE JUEGO
//   devuelve true  → volver a “Cargar mapa”
//            false → volver al menú de inicio
// *************************************************
bool ejecutarJuego(RenderWindow& window,
                   const string& archivoMapa,
                   Font& fuente)
{
    // ---------- Carga del mapa ----------
    ifstream archivo(archivoMapa);
    if(!archivo)
    {
        tinyfd_messageBox("Error",
            ("No pude abrir:\n"+archivoMapa).c_str(),
            "ok","error",1);
        return false;
    }
    vector<string> mapaOriginal; string linea;
    while(getline(archivo,linea)) mapaOriginal.push_back(linea);
    vector<string> mapa = mapaOriginal;

    // ---------- Recursos ----------
    Texture bgTx;  bgTx.loadFromFile("espacio.png");
    Sprite fondo(bgTx); fondo.setScale(
        float(WINDOW_WIDTH)/bgTx.getSize().x,
        float(WINDOW_HEIGHT)/bgTx.getSize().y);

    static Texture txOn,txOff; static bool cargadas=false;
    if(!cargadas){ txOn.loadFromFile("PentagonosON.png");
                   txOff.loadFromFile("PentagonosOFF.png"); cargadas=true;}

    // ---------- Geometría ----------
    size_t filas = mapa.size(), cols = 0;
for (auto& l : mapa) cols = max(cols, l.size());

float escalaX = (WINDOW_WIDTH - 100.f) / (cols * PASO_X_BASE);

float altura = 0.f;
for (size_t i = 0; i < filas; ++i)
    altura += (i % 2 ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE);

float escalaY = (WINDOW_HEIGHT - 150.f) / altura;   //  ← faltaba esta línea
float escala  = std::min(escalaX, escalaY);          //  ← ahora usa escalaY


    float RADIO=RADIO_BASE*escala,
          PASO_X=PASO_X_BASE*escala,
          PASO_Y_PAR=PASO_Y_PAR_BASE*escala,
          PASO_Y_IMP=PASO_Y_IMPAR_BASE*escala;

    map<pair<int,int>,Vector2f> posCelda;
    float yAcc=70.f;
    for(size_t f=0;f<filas;++f)
    {
        for(size_t c=0;c<mapa[f].size();++c)
            posCelda[{(int)f,(int)c}]={c*PASO_X+50.f,yAcc};
        yAcc+=(f%2?PASO_Y_IMP:PASO_Y_PAR);
    }

    // ---------- Entidades ----------
    int fJug=-1,cJug=-1,fMeta=-1,cMeta=-1,movs=0,bateria=BATERIA_MAX;
    CircleShape jugador(RADIO/2),meta(RADIO/2);
    jugador.setFillColor(Color::Red); meta.setFillColor(Color::Green);
    jugador.setOrigin(RADIO/2,RADIO/2); meta.setOrigin(RADIO/2,RADIO/2);

    RectangleShape barraFondo({200,20});
    barraFondo.setFillColor({50,50,50}); barraFondo.setPosition(WINDOW_WIDTH-220,20);
    RectangleShape barraCarga; barraCarga.setPosition(WINDOW_WIDTH-220,20);
    Text txtBateria("Bateria: 20",fuente,16); txtBateria.setPosition(WINDOW_WIDTH-215,45);

    Clock relojMsg; Text msgMeta("",fuente,24); msgMeta.setPosition(20,20);
    bool mostrarMsg=false;

    // ---------- Ruta BFS ----------
    vector<pair<int,int>> ruta; bool mostrarRuta=false, autoMover=false;
    size_t idxRuta=0; Clock relojPaso;

    auto actualizarRuta=[&](){
        ruta=bfsRuta(mapa,fJug,cJug,fMeta,cMeta);
        mostrarRuta=!ruta.empty(); idxRuta=0;
    };

    auto celdaOk=[&](int f,int c)->bool{
        return f>=0 && f<(int)filas &&
               c>=0 && c<(int)mapa[f].size() &&
               mapa[f][c]=='P';
    };

    bool volverMenuCarga=false; // ← bandera de victoria

    // ---------- Movimiento + cambios aleatorios ----------
    auto aplicarCambiosAleatorios=[&](){
        for(size_t i=0;i<filas;++i)
            for(size_t j=0;j<mapa[i].size();++j)
            {
                if((int)i==fJug&&(int)j==cJug) continue;
                if((int)i==fMeta&&(int)j==cMeta) continue;
                if(mapaOriginal[i][j]=='.')      continue;
                if(rand()%100 < CAMBIO_PROBABILIDAD*100)
                    mapa[i][j]=(mapa[i][j]=='P')?'.':'P';
            }
    };

    auto moverJugador=[&](int nf,int nc){
        fJug=nf; cJug=nc; jugador.setPosition(posCelda[{fJug,cJug}]);
        bateria--; movs++; txtBateria.setString("Bateria: "+to_string(bateria));

        /* ────────── VICTORIA ────────── */
        if(fJug==fMeta && cJug==cMeta)
        {
            autoMover=false;
            int resp = tinyfd_messageBox("¡Meta alcanzada!",
                "¡Felicitaciones!\n\n¿Deseas cargar otro mapa?\n(Sí = cargar, No = salir)",
                "yesno","info",1);
            if(resp == 1)   volverMenuCarga = true;   // regresar a Cargar mapa
            else            window.close();           // salir del juego
            return;                                   //  ⮑  ¡no mover la meta!
        }

        /* ────────── META PERIÓDICA (solo si NO hubo victoria) ────────── */
        if(movs % META_CAMBIO_CADA==0)
        {
            vector<pair<int,int>> cand;
            for(size_t i=0;i<filas;++i)
                for(size_t j=0;j<mapa[i].size();++j)
                    if(mapa[i][j]=='P' && !(i==fJug&&j==cJug))
                        cand.emplace_back(i,j);
            if(!cand.empty()){
                auto nm=cand[rand()%cand.size()];
                fMeta=nm.first; cMeta=nm.second;
                meta.setPosition(posCelda[{fMeta,cMeta}]);
            }
        }

        /* ────────── Mutaciones de mapa ────────── */
        vector<string> respaldo=mapa;
        aplicarCambiosAleatorios();

        // --- Verificar conectividad; reconectar si es necesario ---
        auto area=bfsArea(mapa,fJug,cJug);
        bool metaDentro=false;
        for(auto&p:area) if(p.first==fMeta&&p.second==cMeta){ metaDentro=true;break;}

        if(!metaDentro)
        {
            bool ok=forzarReconectar(mapa,fJug,cJug,fMeta,cMeta);
            if(!ok) mapa=respaldo; // revertir si no se logró
        }
    };

    // ---------- Bucle principal ----------
    while(window.isOpen() && !volverMenuCarga)
    {
        Event ev;
        while(window.pollEvent(ev))
        {
            if(ev.type==Event::Closed) window.close();

            // Click para escoger jugador y meta
            if(ev.type==Event::MouseButtonPressed&&ev.mouseButton.button==Mouse::Left)
            {
                Vector2i mp=Mouse::getPosition(window);
                for(auto&[coord,pos]:posCelda)
                {
                    float d=hypot(pos.x-mp.x,pos.y-mp.y);
                    if(d<=RADIO&&mapa[coord.first][coord.second]=='P')
                    {
                        if(fJug==-1){fJug=coord.first;cJug=coord.second;
                                     jugador.setPosition(pos);}
                        else if(fMeta==-1 && !(coord.first==fJug && coord.second==cJug))
                        {fMeta=coord.first;cMeta=coord.second;meta.setPosition(pos);}
                        break;
                    }
                }
            }

            // Movimiento manual un paso
            if(ev.type==Event::KeyPressed && !autoMover &&
               fJug!=-1&&fMeta!=-1&&bateria>0)
            {
                int nf=fJug,nc=cJug; bool mover=false;
                switch(ev.key.code){
                    case Keyboard::W: nf--; mover=true; break;
                    case Keyboard::S: nf++; mover=true; break;
                    case Keyboard::A:
                        if(fJug%2==0){ nf--; nc--; } else { nf++; nc--; } mover=true; break;
                    case Keyboard::D:
                        if(fJug%2==0){ nf--; nc++; } else { nf++; nc++; } mover=true; break;
                    default: break;
                }
                if(mover && celdaOk(nf,nc)){ moverJugador(nf,nc); mostrarRuta=false; }
            }

            // Tecla R: calcular ruta y auto-mover
            if(ev.type==Event::KeyPressed&&ev.key.code==Keyboard::R &&
               fJug!=-1&&fMeta!=-1){
                actualizarRuta(); autoMover=mostrarRuta;
            }
        }

        // ---- Movimiento automático por ruta ----
        if(autoMover && bateria>0 && mostrarRuta && ruta.size()>1)
        {
            // ¿sigue válida?
            bool rutaOk=true;
            for(size_t i=idxRuta;i<ruta.size();++i)
                if(!celdaOk(ruta[i].first,ruta[i].second)){ rutaOk=false;break; }
            if(!rutaOk){ actualizarRuta(); if(!mostrarRuta) autoMover=false; }

            // avanzar
            if(autoMover && relojPaso.getElapsedTime().asMilliseconds()>300 &&
               idxRuta+1<ruta.size())
            {
                int nf=ruta[idxRuta+1].first,
                    nc=ruta[idxRuta+1].second;
                if(celdaOk(nf,nc)){ moverJugador(nf,nc); idxRuta++; }
                relojPaso.restart();
            }
        }

        if(mostrarMsg && relojMsg.getElapsedTime().asSeconds()>2) mostrarMsg=false;

        // ---- Dibujado ----
        window.clear(); window.draw(fondo);

        // pentágonos
        for(size_t f=0;f<filas;++f)
        {
            float rot=f%2?36.f:0.f;
            for(size_t c=0;c<mapa[f].size();++c)
            {
                if(mapaOriginal[f][c]=='.') continue;
                Sprite sp; sp.setTexture(mapa[f][c]=='P'?txOn:txOff);
                float factor=(RADIO*2)/sp.getTexture()->getSize().x;
                sp.setScale(factor,factor);
                sp.setOrigin(sp.getTexture()->getSize().x/2.f,
                             sp.getTexture()->getSize().y/2.f);
                sp.setPosition(posCelda[{(int)f,(int)c}]); sp.setRotation(rot);
                window.draw(sp);
            }
        }

        // puntos amarillos ruta
        if(mostrarRuta && ruta.size()>1)
        {
            CircleShape punto(RADIO/4.f);
            punto.setFillColor(Color::Yellow);
            punto.setOrigin(punto.getRadius(),punto.getRadius());
            for(auto&[f,c]:ruta){ punto.setPosition(posCelda[{f,c}]); window.draw(punto);}
        }

        if(fMeta!=-1) window.draw(meta);
        if(fJug!=-1)  window.draw(jugador);

        // batería
        float w=200.f*bateria/BATERIA_MAX;
        barraCarga.setSize({w,20});
        float perc=float(bateria)/BATERIA_MAX;
        barraCarga.setFillColor( perc>0.66?Color::Green:
                                 perc>0.33?Color::Yellow:Color::Red );
        window.draw(barraFondo); window.draw(barraCarga); window.draw(txtBateria);
        if(mostrarMsg) window.draw(msgMeta);

        window.display();
    }
    return volverMenuCarga;
}

// ============================================================================
//  Menú principal
// ============================================================================
int main()
{
    srand((unsigned)time(nullptr));
    RenderWindow window(VideoMode(WINDOW_WIDTH,WINDOW_HEIGHT),"Escape the Grid");
    window.setFramerateLimit(60);

    Font fuente;  fuente.loadFromFile("arial.ttf");

    Texture bgMenuTx; bgMenuTx.loadFromFile("labo.png");
    Sprite bgMenu(bgMenuTx); bgMenu.setScale(
        float(WINDOW_WIDTH)/bgMenuTx.getSize().x,
        float(WINDOW_HEIGHT)/bgMenuTx.getSize().y);

    RectangleShape btnRect({250,60});
    btnRect.setFillColor(Color(20,20,20,180));
    btnRect.setOrigin(btnRect.getSize().x/2.f, btnRect.getSize().y/2.f);

    Text btnTxt("Jugar",fuente,28); btnTxt.setFillColor(Color::White);

    Estado estado=Estado::MENU_INICIO; string mapaSel;

    while(window.isOpen())
    {
        Event e;
        while(window.pollEvent(e))
        {
            if(e.type==Event::Closed) window.close();

            if(estado==Estado::MENU_INICIO &&
               e.type==Event::MouseButtonPressed && e.mouseButton.button==Mouse::Left)
            {
                Vector2i mp=Mouse::getPosition(window);
                if(btnRect.getGlobalBounds().contains(float(mp.x),float(mp.y)))
                    estado=Estado::MENU_CARGAR;
            }
            else if(estado==Estado::MENU_CARGAR &&
                    e.type==Event::MouseButtonPressed && e.mouseButton.button==Mouse::Left)
            {
                Vector2i mp=Mouse::getPosition(window);
                if(btnRect.getGlobalBounds().contains(float(mp.x),float(mp.y)))
                {
                    const char* filtros[1]={"*.txt"};
                    const char* ruta=tinyfd_openFileDialog("Selecciona nivel","",1,
                                                           filtros,"Nivel",0);
                    if(ruta)
                    {
                        mapaSel=ruta; estado=Estado::JUEGO;
                        bool volver = ejecutarJuego(window,mapaSel,fuente);
                        if(!window.isOpen()) break;              // usuario eligió salir
                        estado = volver ? Estado::MENU_CARGAR    // volver a carga
                                        : Estado::MENU_INICIO;   // volver a inicio
                    }
                }
            }
        }

        if(!window.isOpen()) break;

        if(estado==Estado::MENU_INICIO||estado==Estado::MENU_CARGAR)
        {
            window.clear(); window.draw(bgMenu);

            string textoBoton=(estado==Estado::MENU_INICIO)?"Jugar":"Cargar mapa";
            btnTxt.setString(textoBoton);
            FloatRect b=btnTxt.getLocalBounds();
            btnTxt.setOrigin(b.left+b.width/2.f,b.top+b.height/2.f);

            btnRect.setPosition(WINDOW_WIDTH/2.f,WINDOW_HEIGHT*0.7f);
            btnTxt .setPosition(btnRect.getPosition());

            Text titulo("Escape the Grid",fuente,48);
            titulo.setFillColor(Color::White);
            FloatRect tb=titulo.getLocalBounds();
            titulo.setOrigin(tb.left+tb.width/2.f,tb.top+tb.height/2.f);
            titulo.setPosition(WINDOW_WIDTH/2.f,WINDOW_HEIGHT*0.25f);

            window.draw(btnRect); window.draw(btnTxt); window.draw(titulo);
            window.display();
        }
    }
    return 0;
}
