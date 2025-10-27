# Cliente e Servidor HTTP em C

Implementação de um cliente HTTP (navegador) e servidor HTTP em linguagem C.

## Características

### Cliente HTTP (`meu_navegador`)
- Baixa arquivos de URLs HTTP
- Suporte a redirecionamentos (301, 302)
- Salva arquivos localmente
- Suporte a diferentes tipos de arquivo

### Servidor HTTP (`meu_servidor`)
- Serve arquivos estáticos de um diretório
- Listagem automática de diretórios
- Suporte a arquivo `index.html`
- Tipos MIME automáticos
- Múltiplas conexões simultâneas

## Compilação

```bash
make
