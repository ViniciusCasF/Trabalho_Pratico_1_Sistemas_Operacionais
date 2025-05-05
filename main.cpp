#include <iostream>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

using namespace std;

// Constantes
#define NUMBER_OF_CUSTOMERS 5    // Número total de clientes
#define NUMBER_OF_RESOURCES 3    // Número total de tipos de recursos

// Variáveis globais
int available[NUMBER_OF_RESOURCES];                      // Recursos disponíveis
int maximum[NUMBER_OF_CUSTOMERS][NUMBER_OF_RESOURCES];   // Máximo que cada cliente pode pedir
int allocation[NUMBER_OF_CUSTOMERS][NUMBER_OF_RESOURCES] = {0};  // Recursos atualmente alocados
int need[NUMBER_OF_CUSTOMERS][NUMBER_OF_RESOURCES];      // Necessidade de cada cliente

pthread_mutex_t mutex;  // Mutex para controle de concorrência

// Funções que serão usadas
bool is_safe();
int request_resources(int customer_num, int request[]);
int release_resources(int customer_num, int release[]);
void* customer_thread(void* arg);
void print_state(bool force_print = false);

// Função para imprimir os arrays
void print_array(int arr[], int size = NUMBER_OF_RESOURCES) {
    cout << "[";
    for (int i = 0; i < size; i++) {
        cout << arr[i];
        if (i < size - 1) cout << " ";
    }
    cout << "]";
}

// Verifica se o sistema está em estado seguro
bool is_safe() {
    int work[NUMBER_OF_RESOURCES];                   // Faz uma cópia dos recursos disponíveis
    bool finish[NUMBER_OF_CUSTOMERS] = {false};      // Marca quais clientes já finalizaram

    // Work recebe os recursos disponiveis
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        work[i] = available[i];
    }

    int count = 0;
    while (count < NUMBER_OF_CUSTOMERS) {
        bool found = false;
        // Procura um cliente que possa ser atendido com os recursos atuais
        for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
            if (!finish[i]) {
                bool can_allocate = true;
                // Verifica se a necessidade do cliente é menor ou igual aos recursos disponíveis
                for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
                    if (need[i][j] > work[j]) {
                        can_allocate = false;
                        break;
                    }
                }

                // Se pode alocar, simula a liberação de recursos
                if (can_allocate) {
                    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
                        work[j] += allocation[i][j];
                    }
                    finish[i] = true;
                    found = true;
                    count++;
                }
            }
        }
        if (!found) break;  // Se não encontrou nenhum cliente, sai do loop
    }
    return (count == NUMBER_OF_CUSTOMERS);  // Seguro se todos os clientes puderem terminar
}

// Função para solicitar recursos
int request_resources(int customer_num, int request[]) {
    pthread_mutex_lock(&mutex);  // Bloqueia para evitar condições de corrida

    // Verifica se a solicitação excede a necessidade máxima
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        if (request[i] > need[customer_num][i]) {
            cout << "Cliente " << customer_num << " solicitou ";
            print_array(request);
            cout << " -> NEGADO (excede necessidade máxima)\n";
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }

    // Verifica se há recursos disponíveis suficientes
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        if (request[i] > available[i]) {
            cout << "Cliente " << customer_num << " solicitou ";
            print_array(request);
            cout << " -> NEGADO (recursos insuficientes)\n";
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }

    // Tenta alocar os recursos
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        available[i] -= request[i];
        allocation[customer_num][i] += request[i];
        need[customer_num][i] -= request[i];
    }

    // Verifica se o sistema permanece seguro após a alocação
    if (!is_safe()) {
        cout << "Cliente " << customer_num << " solicitou ";
        print_array(request);
        cout << " -> NEGADO (levaria a estado inseguro)\n";
        for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
            available[i] += request[i];
            allocation[customer_num][i] -= request[i];
            need[customer_num][i] += request[i];
        }
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    // Se chegou aqui, a alocação foi bem-sucedida
    cout << "Cliente " << customer_num << " solicitou ";
    print_array(request);
    cout << " -> APROVADO\n";
    print_state();  // Mostra o estado atual do sistema

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Função para liberar recursos
int release_resources(int customer_num, int release[]) {
    pthread_mutex_lock(&mutex);

    cout << "Cliente " << customer_num << " liberou ";
    print_array(release);
    cout << "\n";

    // Verifica se a liberação é válida
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        if (release[i] > allocation[customer_num][i]) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }

    // Libera os recursos
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        available[i] += release[i];
        allocation[customer_num][i] -= release[i];
        need[customer_num][i] += release[i];
    }

    print_state();  // Atualiza o estado do sistema

    pthread_mutex_unlock(&mutex);
    return 0;
}

// Função para imprimir o estado do sistema
void print_state(bool force_print) {
    static int last_available[NUMBER_OF_RESOURCES];
    static int last_allocation[NUMBER_OF_CUSTOMERS][NUMBER_OF_RESOURCES] = {0};

    bool changed = force_print;

    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        if (available[i] != last_available[i]) {
            changed = true;
            break;
        }
    }

    if (!changed) {
        for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
            for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
                if (allocation[i][j] != last_allocation[i][j]) {
                    changed = true;
                    break;
                }
            }
            if (changed) {
                break;
            }
        }
    }

    if (!changed) return;

    // Atualiza os últimos valores conhecidos
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        last_available[i] = available[i];
    }
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
            last_allocation[i][j] = allocation[i][j];
        }
    }

    // Imprime o estado atual
    cout << "\n=== Estado Atual ===";
    cout << "\nDisponível: ";
    print_array(available);

    cout << "\nAlocações:";
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        bool has_allocation = false;
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
            if (allocation[i][j] > 0) {
                has_allocation = true;
                break;
            }
        }

        if (has_allocation) {
            cout << "\n  Cliente " << i << ": ";
            print_array(allocation[i]);
        }
    }
    cout << "\n===================\n";
}

// Thread do cliente
void* customer_thread(void* arg) {
    int customer_num = *((int*)arg);

    while (true) {  // Loop infinito
        int request[NUMBER_OF_RESOURCES];

        // Gera uma solicitação aleatória baseada na necessidade atual
        for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
            request[i] = (need[customer_num][i] > 0) ? rand() % (need[customer_num][i] + 1) : 0;
        }

        // Tenta solicitar recursos
        if (request_resources(customer_num, request) == 0) {
            usleep(500000); // Espera 500ms antes de liberar
            release_resources(customer_num, request);
        }

        usleep(300000); // Espera 300ms entre solicitações
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    // Verifica os argumentos
    if (argc != NUMBER_OF_RESOURCES + 1) {
        cerr << "Uso: " << argv[0] << " <recurso1> <recurso2> <recurso3>\n";
        return 1;
    }

    srand(time(nullptr));  // Embaralha a seed dos numeros aleatorios

    // Inicializa os recursos disponíveis
    for (int i = 0; i < NUMBER_OF_RESOURCES; i++) {
        available[i] = atoi(argv[i + 1]);
    }

    // Inicializa a matriz de máxima necessidade
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
            maximum[i][j] = rand() % (available[j] + 1);
            need[i][j] = maximum[i][j];
        }
    }

    pthread_mutex_init(&mutex, nullptr);  // Inicializa o mutex

    cout << "=== Sistema Inicializado ===\n";
    cout << "Recursos disponíveis: ";
    print_array(available);
    cout << "\n\n";

    print_state(true);  // Imprime o estado inicial

    // Cria as threads dos clientes
    pthread_t threads[NUMBER_OF_CUSTOMERS];
    int customer_ids[NUMBER_OF_CUSTOMERS];
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        customer_ids[i] = i;
        pthread_create(&threads[i], nullptr, customer_thread, &customer_ids[i]);
    }

    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        pthread_join(threads[i], nullptr);
    }

    pthread_mutex_destroy(&mutex);
    return 0;
}