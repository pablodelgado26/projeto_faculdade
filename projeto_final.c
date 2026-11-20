#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_BOLETOS 5000 // Limite máximo de boletos aceitos

typedef struct {
    char cliente[6];
    char conta[7];
    char num_boleto[9];
    char data_vencimento[9];
    char data_pagamento[9];
    char valor_pago_str[9];
    long long valor_pago;
    int processado;
} Boleto;

// Remove \r e \n do final e retorna 1 se havia quebra de linha, ou 0 se não havia
int limpar_quebra_linha(char *str) {
    int tinha_quebra = 0;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
        tinha_quebra = 1;
    }
    return tinha_quebra;
}

// Registra falhas no ERRO.TXT distinguindo erro de remessa/layout de erro operacional
void registrar_erro(const char *tipo_erro, const char *mensagem) {
    FILE *f_erro = fopen("ERRO.TXT", "w");
    if (f_erro) {
        fprintf(f_erro, "%s:\n%s\n", tipo_erro, mensagem);
        fclose(f_erro);
    }
    printf("Falha registrada em ERRO.TXT (%s).\n", tipo_erro);
    exit(1);
}

void erro_remessa(const char *mensagem) {
    registrar_erro("ERRO NO ARQUIVO DE REMESSA", mensagem);
}

void erro_operacional(const char *mensagem) {
    registrar_erro("ERRO NA GERACAO / SISTEMA", mensagem);
}

// Verifica se toda a string contém apenas dígitos numéricos
int apenas_digitos(const char *str) {
    if (str == NULL || str[0] == '\0') return 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i])) {
            return 0;
        }
    }
    return 1;
}

int eh_bissexto(int ano) {
    return (ano % 4 == 0 && ano % 100 != 0) || (ano % 400 == 0);
}

// Valida data no formato AAAAMMDD (8 dígitos numéricos e data calendário válida)
int data_valida(const char *data) {
    if (strlen(data) != 8 || !apenas_digitos(data)) {
        return 0;
    }

    char ano_str[5], mes_str[3], dia_str[3];
    strncpy(ano_str, data, 4);
    ano_str[4] = '\0';
    strncpy(mes_str, data + 4, 2);
    mes_str[2] = '\0';
    strncpy(dia_str, data + 6, 2);
    dia_str[2] = '\0';

    int ano = atoi(ano_str);
    int mes = atoi(mes_str);
    int dia = atoi(dia_str);

    if (ano < 1900 || ano > 9999) return 0;
    if (mes < 1 || mes > 12) return 0;

    int dias_no_mes[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_dia = dias_no_mes[mes];
    if (mes == 2 && eh_bissexto(ano)) {
        max_dia = 29;
    }

    return (dia >= 1 && dia <= max_dia);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso obrigatorio: %s <arquivo_de_remessa.txt>\n", argv[0]);
        return 1;
    }

    FILE *f_in = fopen(argv[1], "r");
    if (!f_in) {
        erro_operacional("Nao foi possivel abrir o arquivo de remessa especificado.");
    }

    // Buffer amplo para detectar cabeçalhos excessivamente longos
    char linha[1024];

    // Item 6: Validação completa da primeira linha (cabeçalho)
    if (!fgets(linha, sizeof(linha), f_in)) {
        fclose(f_in);
        erro_remessa("Arquivo de remessa encontra-se vazio.");
    }

    int tinha_quebra_cabecalho = limpar_quebra_linha(linha);

    // Se não havia quebra de linha e o buffer encheu, o cabeçalho é maior que o permitido
    if (!tinha_quebra_cabecalho && strlen(linha) == sizeof(linha) - 1) {
        fclose(f_in);
        erro_remessa("Cabecalho excessivamente longo e fora do padrao (esperado 9 caracteres e ID=1).");
    }

    if (strlen(linha) != 9 || linha[0] != '1') {
        fclose(f_in);
        erro_remessa("Cabecalho fora do padrao (esperado 9 caracteres e ID=1).");
    }

    if (!data_valida(linha + 1)) {
        fclose(f_in);
        char msg[120];
        sprintf(msg, "Cabecalho invalido: DATA_GERACAO ('%s') nao e uma data valida.",
                linha + 1);
        erro_remessa(msg);
    }

    Boleto *boletos = malloc((size_t)MAX_BOLETOS * sizeof(Boleto));
    if (!boletos) {
        fclose(f_in);
        erro_operacional("Falha de alocacao de memoria para armazenar os boletos.");
    }

    int total_boletos = 0;
    int num_linha = 2;

    while (fgets(linha, sizeof(linha), f_in)) {
        int tinha_quebra = limpar_quebra_linha(linha);

        // Ignora linha em branco apenas se houver quebra de linha
        if (strlen(linha) == 0 && tinha_quebra) {
            num_linha++;
            continue;
        }

        if (!tinha_quebra && strlen(linha) == sizeof(linha) - 1) {
            char msg[160];
            sprintf(msg, "Linha %d excede o tamanho permitido.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (linha[0] != '2') {
            char msg[120];
            sprintf(msg, "Linha %d invalida: o ID do registro deve ser 2.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (strlen(linha) != 44) {
            char msg[140];
            sprintf(msg, "Linha %d com tamanho incorreto. Esperado 44 caracteres, lido %zu.",
                    num_linha, strlen(linha));
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        // Item 3: Verifica o limite antes de inserir o novo boleto (permite até MAX_BOLETOS)
        if (total_boletos >= MAX_BOLETOS) {
            char msg[140];
            sprintf(msg, "Limite maximo de %d boletos excedido na linha %d.",
                    MAX_BOLETOS, num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        Boleto atual;
        memset(&atual, 0, sizeof(Boleto));

        strncpy(atual.cliente, linha + 1, 5);
        atual.cliente[5] = '\0';

        strncpy(atual.conta, linha + 6, 6);
        atual.conta[6] = '\0';

        strncpy(atual.num_boleto, linha + 12, 8);
        atual.num_boleto[8] = '\0';

        strncpy(atual.data_vencimento, linha + 20, 8);
        atual.data_vencimento[8] = '\0';

        strncpy(atual.data_pagamento, linha + 28, 8);
        atual.data_pagamento[8] = '\0';

        strncpy(atual.valor_pago_str, linha + 36, 8);
        atual.valor_pago_str[8] = '\0';

        // Item 2: Validações numéricas e de datas
        if (!apenas_digitos(atual.cliente)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: campo CLIENTE deve conter apenas numeros.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (!apenas_digitos(atual.conta)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: campo CONTA deve conter apenas numeros.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (!apenas_digitos(atual.num_boleto)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: campo NUM_BOLETO deve conter apenas numeros.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (!data_valida(atual.data_vencimento)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: DATA_VENCIMENTO ('%s') nao e uma data valida.",
                    num_linha, atual.data_vencimento);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (!data_valida(atual.data_pagamento)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: DATA_PAGAMENTO ('%s') nao e uma data valida.",
                    num_linha, atual.data_pagamento);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        if (!apenas_digitos(atual.valor_pago_str)) {
            char msg[120];
            sprintf(msg, "Linha %d invalida: VALOR_PAGO deve conter apenas numeros.", num_linha);
            free(boletos);
            fclose(f_in);
            erro_remessa(msg);
        }

        atual.valor_pago = atoll(atual.valor_pago_str);
        atual.processado = 0;

        boletos[total_boletos] = atual;
        total_boletos++;
        num_linha++;
    }

    fclose(f_in);

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    if (!tm_info) {
        free(boletos);
        erro_operacional("Falha ao obter a data atual do sistema.");
    }

    char data_geracao[9];
    sprintf(data_geracao, "%04d%02d%02d",
            tm_info->tm_year + 1900,
            tm_info->tm_mon + 1,
            tm_info->tm_mday);

    for (int i = 0; i < total_boletos; i++) {
        if (boletos[i].processado) continue;

        char cliente_atual[6];
        strcpy(cliente_atual, boletos[i].cliente);

        char nome_arquivo[64];
        sprintf(nome_arquivo, "%s_%s.txt", data_geracao, cliente_atual);

        FILE *f_out = fopen(nome_arquivo, "w");
        if (!f_out) {
            free(boletos);
            erro_operacional("Falha ao criar arquivo de retorno.");
        }

        if (fprintf(f_out, "1%s\n", data_geracao) < 0) {
            fclose(f_out);
            free(boletos);
            erro_operacional("Falha ao escrever cabecalho no arquivo de retorno.");
        }

        int cont_boletos_cliente = 0;
        long long soma_valor_cliente = 0;

        for (int j = i; j < total_boletos; j++) {
            if (!boletos[j].processado && strcmp(boletos[j].cliente, cliente_atual) == 0) {
                // Item 4: Garante que o VALOR_TOTAL não ultrapasse 10 dígitos (9.999.999.999)
                if (soma_valor_cliente > 9999999999LL - boletos[j].valor_pago) {
                    fclose(f_out);
                    free(boletos);
                    char msg[150];
                    sprintf(msg, "Soma do VALOR_TOTAL para o cliente %s ultrapassa 10 digitos (9999999999).",
                            cliente_atual);
                    erro_remessa(msg);
                }

                if (fprintf(f_out, "2%s%s%s%s\n",
                            boletos[j].conta,
                            boletos[j].num_boleto,
                            boletos[j].data_pagamento,
                            boletos[j].valor_pago_str) < 0) {
                    fclose(f_out);
                    free(boletos);
                    erro_operacional("Falha ao escrever registro de boleto no arquivo de retorno.");
                }

                cont_boletos_cliente++;
                soma_valor_cliente += boletos[j].valor_pago;
                boletos[j].processado = 1;
            }
        }

        if (fprintf(f_out, "3%06d%010lld\n", cont_boletos_cliente, soma_valor_cliente) < 0) {
            fclose(f_out);
            free(boletos);
            erro_operacional("Falha ao escrever rodape no arquivo de retorno.");
        }

        if (fclose(f_out) != 0) {
            free(boletos);
            erro_operacional("Falha ao fechar arquivo de retorno.");
        }
    }

    free(boletos);
    printf("Processamento concluido! Arquivos de retorno gerados com sucesso.\n");
    return 0;
}