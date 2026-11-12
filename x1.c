/*
 * Projeto x1 - Adivinhe o Numero do Vilao
 * ---------------------------------------------------------------
 * Voce foi capturado pelo vilao Cipher. Para escapar, hackeie o
 * terminal dele adivinhando o codigo de acesso.
 *
 * Compilar: gcc -std=c99 -Wall -Wextra -pedantic -o x1 x1.c
 *
 * ESTRUTURAS DE DADOS ESCOLHIDAS
 *  - Jogador : struct pedida no enunciado (nome + tentativas).
 *  - Ranking : struct que "embrulha" um vetor estatico de 10 Jogadores
 *              e um contador. Como o vetor tem tamanho fixo e pequeno
 *              (top 10), nao precisa de alocacao dinamica. O vetor e
 *              mantido SEMPRE ordenado (insercao ordenada), entao exibir
 *              o ranking e apenas percorrer do indice 0 ao total-1.
 *  - Nivel   : struct com nome e valor maximo de cada dificuldade,
 *              guardada em uma tabela constante (nao e o ranking, que
 *              nunca e global: ele vive na main e e passado por ponteiro).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define TAM_NOME    50
#define MAX_RANKING 10
#define NUM_NIVEIS  3

/* Registro de um jogador no ranking. */
typedef struct {
    char nome[TAM_NOME];
    int  tentativas;
} Jogador;

/* Ranking de um nivel: vetor ordenado (menor tentativas primeiro). */
typedef struct {
    Jogador itens[MAX_RANKING];
    int     total;
} Ranking;

/* Dificuldade: nome exibido e maior numero possivel. */
typedef struct {
    const char *nome;
    int         maximo;
} Nivel;

static const Nivel NIVEIS[NUM_NIVEIS] = {
    { "Facil   (1-100)",   100   },
    { "Medio   (1-1000)",  1000  },
    { "Dificil (1-10000)", 10000 }
};

/* ================================================================
 * Entrada de dados (validacao sem travar o programa)
 * ================================================================ */

/* Descarta o que sobrou na linha de entrada. */
static void limpar_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* descartando */
    }
}

/* Se a entrada acabar (Ctrl+D / Ctrl+Z), encerra sem loop infinito. */
static void encerrar_por_eof(void)
{
    printf("\n[Cipher] Conexao encerrada. Ate logo.\n");
    exit(EXIT_SUCCESS);
}

/* Le um inteiro da linha. Retorna 1 se a linha era so um inteiro,
 * 0 se tinha letras/lixo (a linha inteira e descartada). */
static int ler_inteiro(int *valor)
{
    int c;
    int ok = scanf("%d", valor);

    if (ok == EOF) {
        encerrar_por_eof();
    }
    while ((c = getchar()) != '\n') {
        if (c == EOF) {
            if (ok != 1) {
                encerrar_por_eof();
            }
            break;
        }
        if (!isspace(c)) {
            ok = 0; /* algo alem do numero, ex.: "12abc" */
        }
    }
    return ok == 1;
}

/* Repete ate o usuario digitar um inteiro dentro de [min, max]. */
static int ler_no_intervalo(const char *prompt, int min, int max)
{
    int valor;

    for (;;) {
        printf("%s", prompt);
        if (!ler_inteiro(&valor)) {
            printf("[ERRO] Digite apenas numeros inteiros.\n");
        } else if (valor < min || valor > max) {
            printf("[ERRO] Valor fora do intervalo [%d, %d].\n", min, max);
        } else {
            return valor;
        }
    }
}

/* Le exatamente um caractere '>', '<' ou '=' (modo IA). */
static char ler_resposta(int chute)
{
    int c, prox;

    for (;;) {
        printf("Cipher-IA chuta %d. Seu numero e > (maior), < (menor) ou = (acertou)? ",
               chute);
        c = getchar();
        if (c == EOF) {
            encerrar_por_eof();
        }
        if (c != '\n') {
            prox = getchar();
            if (prox != '\n') {
                if (prox != EOF) {
                    limpar_buffer();
                }
                c = 0; /* mais de um caractere: invalido */
            }
        }
        if (c == '>' || c == '<' || c == '=') {
            return (char)c;
        }
        printf("[ERRO] Responda apenas com >, < ou =.\n");
    }
}

/* Le o nome do jogador com fgets (aceita espacos). */
static void ler_nome(char *dest, size_t tam)
{
    size_t len;

    printf("Nome do hacker para o ranking: ");
    if (fgets(dest, (int)tam, stdin) == NULL) {
        encerrar_por_eof();
    }
    len = strlen(dest);
    if (len > 0 && dest[len - 1] == '\n') {
        dest[len - 1] = '\0';
    } else {
        limpar_buffer(); /* nome maior que o buffer: descarta o resto */
    }
    if (dest[0] == '\0') {
        strcpy(dest, "Anonimo");
    }
}

/* ================================================================
 * Regras do jogo
 * ================================================================ */

/* Tentativas permitidas = ceil(log2(maximo)), calculado por inteiros
 * (menor t tal que 2^t >= maximo). Facil=7, Medio=10, Dificil=14. */
static int tentativas_maximas(int maximo)
{
    int t = 0;
    int potencia = 1;

    while (potencia < maximo) {
        potencia *= 2;
        t++;
    }
    return t;
}

/* Sorteia um numero em [1, maximo]. srand e chamado uma vez na main. */
static int sortear(int maximo)
{
    return rand() % maximo + 1;
}

static void dar_dica(int palpite, int secreto)
{
    if (palpite < secreto) {
        printf("[Cipher] O codigo e MAIOR.\n");
    } else if (palpite > secreto) {
        printf("[Cipher] O codigo e MENOR.\n");
    } else {
        printf("[Cipher] ACERTOU! Acesso concedido.\n");
    }
}

static int ler_palpite(int maximo, int restantes)
{
    char prompt[96];

    snprintf(prompt, sizeof prompt,
             "Tentativa (restam %d) - palpite [1-%d]: ", restantes, maximo);
    return ler_no_intervalo(prompt, 1, maximo);
}

/* ================================================================
 * Ranking
 * ================================================================ */

/* Insercao ordenada: coloca o novo resultado no fim (ou no lugar do
 * pior, se cheio) e o "sobe" ate a posicao certa (insertion sort).
 * Retorna 1 se entrou no ranking, 0 se nao era bom o bastante. */
static int inserir_ranking(Ranking *r, const char *nome, int tentativas)
{
    int pos;
    Jogador tmp;

    if (r->total == MAX_RANKING) {
        if (tentativas >= r->itens[MAX_RANKING - 1].tentativas) {
            return 0;
        }
        pos = MAX_RANKING - 1; /* substitui o pior */
    } else {
        pos = r->total++;
    }
    strncpy(r->itens[pos].nome, nome, TAM_NOME - 1);
    r->itens[pos].nome[TAM_NOME - 1] = '\0';
    r->itens[pos].tentativas = tentativas;

    while (pos > 0 && r->itens[pos - 1].tentativas > r->itens[pos].tentativas) {
        tmp = r->itens[pos - 1];
        r->itens[pos - 1] = r->itens[pos];
        r->itens[pos] = tmp;
        pos--;
    }
    return 1;
}

static void exibir_ranking(const Ranking *r, const char *titulo)
{
    int i;

    printf("\n=== RANKING - %s ===\n", titulo);
    if (r->total == 0) {
        printf("  (nenhum hacker conseguiu invadir ainda)\n");
        return;
    }
    printf("  %-3s %-30s %s\n", "#", "Nome", "Tentativas");
    for (i = 0; i < r->total; i++) {
        printf("  %-3d %-30s %d\n", i + 1, r->itens[i].nome, r->itens[i].tentativas);
    }
}

static void exibir_todos_rankings(const Ranking rankings[])
{
    int i;

    for (i = 0; i < NUM_NIVEIS; i++) {
        exibir_ranking(&rankings[i], NIVEIS[i].nome);
    }
}

/* ================================================================
 * Modos de jogo
 * ================================================================ */

/* Modo 1: o humano adivinha o numero sorteado pelo computador. */
static void jogar_humano(Ranking *r, const Nivel *nivel)
{
    int limite = tentativas_maximas(nivel->maximo);
    int secreto = sortear(nivel->maximo);
    int tentativas = 0;
    int palpite = 0;
    char nome[TAM_NOME];

    printf("\n[Cipher] Adivinhe o codigo entre 1 e %d. Voce tem %d tentativas.\n",
           nivel->maximo, limite);
    do {
        palpite = ler_palpite(nivel->maximo, limite - tentativas);
        tentativas++;
        dar_dica(palpite, secreto);
    } while (palpite != secreto && tentativas < limite);

    if (palpite == secreto) {
        printf("Voce hackeou o terminal em %d tentativa(s)!\n", tentativas);
        ler_nome(nome, sizeof nome);
        if (inserir_ranking(r, nome, tentativas)) {
            printf("Resultado registrado no ranking!\n");
        } else {
            printf("Nao foi dessa vez: voce nao entrou no top %d.\n", MAX_RANKING);
        }
    } else {
        printf("[Cipher] Alarme! Suas tentativas acabaram. O codigo era %d.\n", secreto);
    }
}

/* Modo 2: o computador adivinha usando BUSCA BINARIA verdadeira.
 * Invariante: o numero do humano esta sempre em [inicio, fim]. */
static void jogar_ia(const Nivel *nivel)
{
    int inicio = 1;
    int fim = nivel->maximo;
    int meio = 0;
    int chutes = 0;
    int limite = tentativas_maximas(nivel->maximo);
    char resposta = 0;

    printf("\n[Cipher-IA] Pense em um numero de 1 a %d e nao me conte.\n", nivel->maximo);
    printf("Pressione ENTER quando estiver pronto...");
    limpar_buffer();

    while (inicio <= fim && resposta != '=') {
        meio = inicio + (fim - inicio) / 2; /* evita overflow de (inicio+fim) */
        chutes++;
        resposta = ler_resposta(meio);
        if (resposta == '>') {
            inicio = meio + 1;
        } else if (resposta == '<') {
            fim = meio - 1;
        }
    }

    if (resposta == '=') {
        printf("[Cipher-IA] Descobri o numero %d em %d chute(s) (maximo esperado: %d).\n",
               meio, chutes, limite);
    } else {
        printf("[Cipher-IA] Suas respostas foram inconsistentes: nao ha numero possivel!\n");
    }
}

/* ================================================================
 * Menus
 * ================================================================ */

static void exibir_banner(void)
{
    printf("==============================================\n");
    printf("   TERMINAL DO VILAO CIPHER - ACESSO NEGADO   \n");
    printf("==============================================\n");
}

static void exibir_menu(void)
{
    printf("\n1 - Jogar (voce adivinha)\n");
    printf("2 - Modo IA (o computador adivinha)\n");
    printf("3 - Ver ranking\n");
    printf("0 - Sair\n");
}

/* Retorna o indice (0..2) do nivel escolhido. */
static int escolher_nivel(void)
{
    int i;

    printf("\nDificuldade:\n");
    for (i = 0; i < NUM_NIVEIS; i++) {
        printf("  %d - %s | %d tentativas\n", i + 1, NIVEIS[i].nome,
               tentativas_maximas(NIVEIS[i].maximo));
    }
    return ler_no_intervalo("Nivel: ", 1, NUM_NIVEIS) - 1;
}

int main(void)
{
    Ranking rankings[NUM_NIVEIS]; /* um ranking por nivel, local (nao global) */
    int opcao, nivel;

    memset(rankings, 0, sizeof rankings);
    srand((unsigned)time(NULL));
    exibir_banner();

    do {
        exibir_menu();
        opcao = ler_no_intervalo("Opcao: ", 0, 3);
        switch (opcao) {
        case 1:
            nivel = escolher_nivel();
            jogar_humano(&rankings[nivel], &NIVEIS[nivel]);
            break;
        case 2:
            nivel = escolher_nivel();
            jogar_ia(&NIVEIS[nivel]);
            break;
        case 3:
            exibir_todos_rankings(rankings);
            break;
        default:
            printf("\n[Cipher] Desconectando... voce nao me pegou desta vez.\n");
            break;
        }
    } while (opcao != 0);

    return EXIT_SUCCESS;
}
