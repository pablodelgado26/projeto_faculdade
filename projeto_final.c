#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_BOLETOS 5000 // Limite de registros em memória

// Estrutura para extrair e armazenar cada campo do boleto
typedef struct {
    char cliente[6];
    char conta[7];
    char num_boleto[9];
    char data_vencimento[9];
    char data_pagamento[9];
    char valor_pago_str[9];
    long long valor_pago; // Facilita a soma sem erros de flutuação
    int processado;       // Flag para o momento da geração agrupada
} Boleto;

// Remove quebras de linha (\r e \n) do final das strings de leitura
void limpar_quebra_linha(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

// Cria o arquivo ERRO.TXT e aborta o programa (Regra 1)
void gerar_erro(const char *mensagem) {
    FILE *f_erro = fopen("ERRO.TXT", "w");
    if (f_erro) {
        fprintf(f_erro, "ERRO ENCONTRADO NO ARQUIVO DE REMESSA:\n%s\n", mensagem);
        fclose(f_erro);
    }
    printf("Erro de layout detectado. Verifique o arquivo ERRO.TXT gerado.\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Regra 3: Obriga uso da linha de comando, sem menus interativos
    if (argc != 2) {
        printf("Uso obrigatorio: %s <arquivo_de_remessa.txt>\n", argv[0]);
        return 1;
    }

    FILE *f_in = fopen(argv[1], "r");
    if (!f_in) {
        gerar_erro("Nao foi possivel abrir o arquivo de remessa especificado.");
    }

    char linha[256];

    // Checagem da Linha 1 (Cabeçalho da Remessa)
    if (!fgets(linha, sizeof(linha), f_in)) {
        gerar_erro("Arquivo de remessa encontra-se vazio.");
    }
    limpar_quebra_linha(linha);

    // Validação do tamanho e ID do cabeçalho (9 caracteres, início '1')
    if (strlen(linha) != 9 || linha[0] != '1') {
        gerar_erro("Cabecalho fora do padrao (esperado 9 caracteres e ID=1).");
    }

    Boleto *boletos = malloc(MAX_BOLETOS * sizeof(Boleto));
    if (!boletos) gerar_erro("Falha de alocacao de memoria.");
    
    int total_boletos = 0;
    int num_linha = 2; // Cabeçalho foi lido na linha 1

    // Checagem e Parse das Linhas Seguintes (Boletos Pagos)
    while (fgets(linha, sizeof(linha), f_in)) {
        limpar_quebra_linha(linha);
        
        if (strlen(linha) == 0) continue; // Ignora linhas em branco soltas no fim do TXT

        // Validação da estrutura das linhas de pagamento
        if (linha[0] != '2') {
            char msg[100];
            sprintf(msg, "Linha %d invalida: O ID do registro deve ser 2.", num_linha);
            gerar_erro(msg);
        }
        if (strlen(linha) != 44) {
            char msg[100];
            sprintf(msg, "Linha %d com tamanho incorreto. Esperado 44 caracteres, lido %zu.", num_linha, strlen(linha));
            gerar_erro(msg);
        }

        // Fatiamento dos campos utilizando as posições fixas do PDF
        strncpy(boletos[total_boletos].cliente, linha + 1, 5);
        boletos[total_boletos].cliente[5] = '\0';

        strncpy(boletos[total_boletos].conta, linha + 6, 6);
        boletos[total_boletos].conta[6] = '\0';

        strncpy(boletos[total_boletos].num_boleto, linha + 12, 8);
        boletos[total_boletos].num_boleto[8] = '\0';
        
        strncpy(boletos[total_boletos].data_vencimento, linha + 20, 8);
        boletos[total_boletos].data_vencimento[8] = '\0';

        strncpy(boletos[total_boletos].data_pagamento, linha + 28, 8);
        boletos[total_boletos].data_pagamento[8] = '\0';

        strncpy(boletos[total_boletos].valor_pago_str, linha + 36, 8);
        boletos[total_boletos].valor_pago_str[8] = '\0';

        boletos[total_boletos].valor_pago = atoll(boletos[total_boletos].valor_pago_str);
        boletos[total_boletos].processado = 0;

        total_boletos++;
        num_linha++;
        
        if (total_boletos >= MAX_BOLETOS) {
            gerar_erro("Limite de capacidade de boletos (MAX_BOLETOS) excedido no script.");
        }
    }
    fclose(f_in);

    // Obtém data do sistema atual para "data de geração" (formato AAAAMMDD)
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char data_geracao[9];
    sprintf(data_geracao, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

    // Regra 2: Gera os arquivos `RETORNO` agrupando todos os boletos por cliente
    for (int i = 0; i < total_boletos; i++) {
        if (boletos[i].processado) continue;

        char cliente_atual[6];
        strcpy(cliente_atual, boletos[i].cliente);

        char nome_arquivo[50];
        sprintf(nome_arquivo, "%s_%s.txt", data_geracao, cliente_atual);

        FILE *f_out = fopen(nome_arquivo, "w");
        if (!f_out) gerar_erro("Falha ao criar o arquivo de retorno.");

        // Grava o Cabeçalho de Retorno (Tamanho 9)
        fprintf(f_out, "1%s\n", data_geracao);

        int cont_boletos_cliente = 0;
        long long soma_valor_cliente = 0;

        // Vare a lista de boletos carregados vinculando ao cliente atual
        for (int j = i; j < total_boletos; j++) {
            if (!boletos[j].processado && strcmp(boletos[j].cliente, cliente_atual) == 0) {
                
                // Grava o Boleto Pago (Tamanho 31)
                fprintf(f_out, "2%s%s%s%s\n", 
                        boletos[j].conta, 
                        boletos[j].num_boleto, 
                        boletos[j].data_pagamento, 
                        boletos[j].valor_pago_str);

                cont_boletos_cliente++;
                soma_valor_cliente += boletos[j].valor_pago;
                boletos[j].processado = 1; // Marca como lido para não repetir
            }
        }

        // Grava a Última linha (Rodapé) - %06d e %010lld garantem os zeros à esquerda (Tamanho 17)
        fprintf(f_out, "3%06d%010lld\n", cont_boletos_cliente, soma_valor_cliente);
        fclose(f_out);
    }

    free(boletos);
    printf("Processamento concluido! Arquivos de retorno gerados com sucesso.\n");
    return 0;
}