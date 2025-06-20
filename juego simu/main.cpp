#include <SFML/Graphics.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <iostream>
#include <cstdlib>
#include <ctime>

const float PI = 3.14159265f;
const float RADIO_BASE = 43.f;
const float PASO_X_BASE = 30.2671f;
const float PASO_Y_PAR_BASE = 48.f;
const float PASO_Y_IMPAR_BASE = 40.f;
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 900;
const int BATERIA_MAX = 20;
const float CAMBIO_PROBABILIDAD = 0.05f;
const int META_CAMBIO_CADA = 5; // N movimientos

sf::ConvexShape crearPentagono(float radio, float rotacion = 0.f) {
    sf::ConvexShape shape;
    shape.setPointCount(5);
    for (int i = 0; i < 5; ++i) {
        float angle = rotacion + 2 * PI * i / 5 - PI / 2;
        shape.setPoint(i, sf::Vector2f(std::cos(angle) * radio, std::sin(angle) * radio));
    }
    shape.setFillColor(sf::Color::Cyan);
    shape.setOutlineColor(sf::Color::Black);
    shape.setOutlineThickness(1);
    return shape;
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Escape the Grid - Bateria");

    sf::Texture texturaFondo;
    texturaFondo.loadFromFile("espacio.png");
    sf::Sprite fondo(texturaFondo);
    sf::Vector2u tamImagen = texturaFondo.getSize();
    fondo.setScale(
        float(WINDOW_WIDTH) / tamImagen.x,
        float(WINDOW_HEIGHT) / tamImagen.y
    );

    std::ifstream archivo("nivel.txt");
    std::vector<std::string> mapaOriginal;
    std::string linea;
    while (std::getline(archivo, linea)) {
        mapaOriginal.push_back(linea);
    }
    std::vector<std::string> mapa = mapaOriginal;

    size_t filas = mapa.size();
    size_t columnas = 0;
    for (const auto& l : mapa)
        if (l.size() > columnas)
            columnas = l.size();

    float alturaTablero = 0.f;
    for (size_t i = 0; i < filas; ++i)
        alturaTablero += (i % 2 == 1) ? PASO_Y_IMPAR_BASE : PASO_Y_PAR_BASE;
    float anchoTablero = columnas * PASO_X_BASE;

    float escalaX = (WINDOW_WIDTH - 100.f) / anchoTablero;
    float escalaY = (WINDOW_HEIGHT - 150.f) / alturaTablero;
    float escala = std::min(escalaX, escalaY);

    float RADIO = RADIO_BASE * escala;
    float PASO_X = PASO_X_BASE * escala;
    float PASO_Y_PAR = PASO_Y_PAR_BASE * escala;
    float PASO_Y_IMPAR = PASO_Y_IMPAR_BASE * escala;

    std::map<std::pair<int, int>, sf::Vector2f> posiciones;
    float yAcumulado = 70.f;

    for (size_t fila = 0; fila < mapa.size(); ++fila) {
        float rotacion = (fila % 2 == 1) ? PI / 5 : 0.f;
        for (size_t col = 0; col < mapa[fila].size(); ++col) {
            float dx = 0.f;
            float cx = col * PASO_X + 50 + dx;
            float cy = yAcumulado;
            posiciones[{(int)fila, (int)col}] = {cx, cy};
        }
        yAcumulado += (fila % 2 == 1) ? PASO_Y_IMPAR : PASO_Y_PAR;
    }

    int filaJugador = -1, colJugador = -1;
    int filaMeta = -1, colMeta = -1;
    int contadorMovimientos = 0;

    sf::CircleShape jugador(RADIO / 2);
    jugador.setFillColor(sf::Color::Red);
    jugador.setOrigin(RADIO / 2, RADIO / 2);

    sf::CircleShape meta(RADIO / 2);
    meta.setFillColor(sf::Color::Green);
    meta.setOrigin(RADIO / 2, RADIO / 2);

    int bateria = BATERIA_MAX;

    sf::Font fuente;
    fuente.loadFromFile("arial.ttf");
    sf::RectangleShape barraFondo(sf::Vector2f(200, 20));
    barraFondo.setFillColor(sf::Color(50, 50, 50));
    barraFondo.setPosition(WINDOW_WIDTH - 220, 20);
    sf::RectangleShape barraCarga;
    barraCarga.setPosition(WINDOW_WIDTH - 220, 20);
    sf::Text textoBateria;
    textoBateria.setFont(fuente);
    textoBateria.setCharacterSize(16);
    textoBateria.setFillColor(sf::Color::White);
    textoBateria.setPosition(WINDOW_WIDTH - 215, 45);
    textoBateria.setString("Bateria: " + std::to_string(bateria));

    sf::Text mensajeMeta;
    mensajeMeta.setFont(fuente);
    mensajeMeta.setCharacterSize(24);
    mensajeMeta.setFillColor(sf::Color::White);
    mensajeMeta.setPosition(20, 20);
    bool mostrarMensaje = false;
    sf::Clock relojMensaje;

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();
            else if (e.type == sf::Event::MouseButtonPressed) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                for (const auto& [coord, pos] : posiciones) {
                    float dist = std::hypot(pos.x - mousePos.x, pos.y - mousePos.y);
                    if (dist <= RADIO && mapa[coord.first][coord.second] == 'P') {
                        if (filaJugador == -1) {
                            filaJugador = coord.first;
                            colJugador = coord.second;
                            jugador.setPosition(pos);
                        } else if (filaMeta == -1 && !(coord.first == filaJugador && coord.second == colJugador)) {
                            filaMeta = coord.first;
                            colMeta = coord.second;
                            meta.setPosition(pos);
                        }
                        break;
                    }
                }
            }
        }

        if (filaJugador != -1 && colJugador != -1 && filaMeta != -1 && colMeta != -1 && bateria > 0) {
            bool mover = false;
            int nuevaFila = filaJugador;
            int nuevaCol = colJugador;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
                nuevaFila--;
                mover = true;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
                nuevaFila++;
                mover = true;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
                if (filaJugador % 2 == 0) {
                    nuevaFila--;
                    nuevaCol--;
                } else {
                    nuevaFila++;
                    nuevaCol--;
                }
                mover = true;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
                if (filaJugador % 2 == 0) {
                    nuevaFila--;
                    nuevaCol++;
                } else {
                    nuevaFila++;
                    nuevaCol++;
                }
                mover = true;
            }

            if (mover && nuevaFila >= 0 && nuevaFila < (int)mapa.size() &&
                nuevaCol >= 0 && nuevaCol < (int)mapa[nuevaFila].size() &&
                mapa[nuevaFila][nuevaCol] == 'P') {

                filaJugador = nuevaFila;
                colJugador = nuevaCol;
                jugador.setPosition(posiciones[{filaJugador, colJugador}]);
                bateria--;
                contadorMovimientos++;
                textoBateria.setString("Bateria: " + std::to_string(bateria));

                if (filaJugador == filaMeta && colJugador == colMeta) {
                    mostrarMensaje = true;
                    relojMensaje.restart();
                    mensajeMeta.setString("\u00a1Meta alcanzada!");
                }

                if (contadorMovimientos % META_CAMBIO_CADA == 0) {
                    std::vector<std::pair<int, int>> posibles;
                    for (size_t i = 0; i < mapa.size(); ++i) {
                        for (size_t j = 0; j < mapa[i].size(); ++j) {
                            if (mapa[i][j] == 'P' && !(i == filaJugador && j == colJugador)) {
                                posibles.emplace_back(i, j);
                            }
                        }
                    }
                    if (!posibles.empty()) {
                        auto nueva = posibles[std::rand() % posibles.size()];
                        filaMeta = nueva.first;
                        colMeta = nueva.second;
                        meta.setPosition(posiciones[{filaMeta, colMeta}]);
                    }
                }

                for (size_t i = 0; i < mapa.size(); ++i) {
                    for (size_t j = 0; j < mapa[i].size(); ++j) {
                        if ((int)i == filaJugador && (int)j == colJugador) continue;
                        if ((int)i == filaMeta && (int)j == colMeta) continue;
                        if (mapaOriginal[i][j] == '.') continue;
                        if ((std::rand() % 100) < (CAMBIO_PROBABILIDAD * 100)) {
                            mapa[i][j] = (mapa[i][j] == 'P') ? '.' : 'P';
                        }
                    }
                }

                sf::sleep(sf::milliseconds(150));
            }
        }

        if (mostrarMensaje && relojMensaje.getElapsedTime().asSeconds() > 2.f) {
            mostrarMensaje = false;
        }

        window.clear();
        window.draw(fondo);
        /*for (size_t fila = 0; fila < mapa.size(); ++fila) {
            float rotacion = (fila % 2 == 1) ? PI / 5 : 0.f;
            for (size_t col = 0; col < mapa[fila].size(); ++col) {
                if (mapa[fila][col] != 'P') continue;
                sf::ConvexShape p = crearPentagono(RADIO, rotacion);
                p.setPosition(posiciones[{(int)fila, (int)col}]);
                window.draw(p);
            }
        }*/
       // --- DIBUJAR EL TABLERO CON SPRITES ROTADOS ------------------------------
static sf::Texture texturaON, texturaOFF;
static bool cargadas = false;
if (!cargadas) {
    texturaON.loadFromFile("PentagonosON.png");
    texturaOFF.loadFromFile("PentagonosOFF.png");
    cargadas = true;
}

for (size_t fila = 0; fila < mapa.size(); ++fila) {
    // 0 rad o 36 ° (π/5) según la fila, igual que antes
    float rotDeg = (fila % 2 == 1) ? 36.f : 0.f;   // grados

    for (size_t col = 0; col < mapa[fila].size(); ++col) {
        if (mapaOriginal[fila][col] == '.') continue;   // puntos fijos no se dibujan

        sf::Sprite sprite;
        sprite.setTexture(mapa[fila][col] == 'P' ? texturaON : texturaOFF);

        // Colocar en la misma posición pre-calculada
        sprite.setPosition(posiciones[{(int)fila, (int)col}]);

        // Escalar para que el sprite “encaje” en el radio calculado
        float factor = (RADIO * 2) / sprite.getTexture()->getSize().x;
        sprite.setScale(factor, factor);

        // Centrar ancla y rotar
        sprite.setOrigin(
            sprite.getTexture()->getSize().x / 2.f,
            sprite.getTexture()->getSize().y / 2.f
        );
        sprite.setRotation(rotDeg);

        window.draw(sprite);
    }
}


        if (filaMeta != -1) window.draw(meta);
        if (filaJugador != -1) window.draw(jugador);

        float anchoActual = 200.f * bateria / BATERIA_MAX;
        barraCarga.setSize(sf::Vector2f(anchoActual, 20));
        float porcentaje = float(bateria) / float(BATERIA_MAX);
        if (porcentaje > 0.66f) barraCarga.setFillColor(sf::Color::Green);
        else if (porcentaje > 0.33f) barraCarga.setFillColor(sf::Color::Yellow);
        else barraCarga.setFillColor(sf::Color::Red);

        window.draw(barraFondo);
        window.draw(barraCarga);
        window.draw(textoBateria);

        if (mostrarMensaje) window.draw(mensajeMeta);

        window.display();
    }
    return 0;
}
