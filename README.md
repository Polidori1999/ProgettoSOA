# Syscall Throttling - Linux Kernel Module

Progetto sviluppato per il corso di **Advanced Operating Systems (and System Security)**, A.A. 2025-2026, Università degli Studi di Roma Tor Vergata.

Il progetto implementa un **Linux Kernel Module (LKM)** per il throttling delle system call su architettura x86-64.

Il modulo permette di configurare tramite un controller user-space quali programmi, EUID e system call controllare. Un monitor applica un budget globale `MAX` di ammissioni per finestra di un secondo, sospendendo temporaneamente i thread quando la quota è esaurita.

## Funzionalità principali

Il progetto supporta:

- registrazione e deregistrazione di UID, nomi di programmi e numeri di system call tramite `ioctl`;
- consultazione degli UID, programmi e numeri di system call registrati;
- modifica della configurazione riservata a processi con effective UID pari a `0`;
- configurazione del limite globale `MAX`;
- attivazione e disattivazione dinamica del monitor;
- finestre temporali autonome e consecutive di un secondo;
- blocco temporaneo dei thread quando viene raggiunto `MAX`;
- rivalidazione della configurazione quando un thread bloccato viene risvegliato;
- gestione di system call bloccanti, non bloccanti e di system call che non ritornano al chiamante;
- raccolta delle statistiche richieste dalla specifica;
- controller user-space per configurare e interrogare il modulo;
- demo riproducibili dei principali comportamenti del monitor.

## Struttura del repository

```text
.
├── Makefile
├── demos/
│   ├── include/
│   │   └── demo_common.h
│   ├── demo_prog.c
│   ├── demo_concurrency.c
│   ├── demo_blocking.c
│   ├── run_demo1.sh
│   ├── run_demo2.sh
│   ├── run_demo3.sh
│   ├── run_demo4.sh
│   ├── run_demo5.sh
│   └── run_demo6.sh
├── include/
│   └── syscall_throttle_ioctl.h
├── kernel/
│   ├── Makefile
│   ├── include/
│   │   ├── access_control.h
│   │   ├── accounting.h
│   │   ├── config.h
│   │   ├── dispatcher_hook.h
│   │   ├── program_registry.h
│   │   ├── statistics.h
│   │   ├── syscall_registry.h
│   │   ├── throttle_engine.h
│   │   └── uid_registry.h
│   ├── accounting.c
│   ├── config.c
│   ├── dispatcher_entry.S
│   ├── dispatcher_hook.c
│   ├── program_registry.c
│   ├── statistics.c
│   ├── syscall_registry.c
│   ├── syscall_throttle_main.c
│   ├── throttle_engine.c
│   └── uid_registry.c
└── user/
    ├── controller.c
    ├── controller.h
    └── main.c
```

Le directory principali hanno i seguenti ruoli:

- `include/`: interfaccia condivisa tra kernel-space e user-space; `syscall_throttle_ioctl.h` definisce le strutture dati e le richieste `ioctl` usate dal modulo, dal controller e dalle demo;
- `kernel/`: implementazione del modulo, dei registri, del monitor, dell’intercettazione delle system call e delle statistiche;
- `kernel/include/`: header interni che dichiarano le funzioni e le strutture condivise tra i componenti del modulo kernel;
- `user/`: controller user-space utilizzato per configurare e interrogare il modulo;
- `demos/`: programmi e script utilizzati per le dimostrazioni interattive del comportamento del monitor; `demos/include/demo_common.h` raccoglie le utilità condivise per misurare i tempi e gestire errori e barriere pthread.

## Flusso di funzionamento

Il controller user-space invia i comandi di configurazione tramite `ioctl` al device `/dev/syscall_throttle`. Il driver kernel li smista ai componenti che gestiscono registri, MAX, stato del monitor e statistiche.

Quando viene invocata una syscall registrata, una Kprobe sul dispatcher `x64_sys_call` redirige l'esecuzione verso un **trampoline assembly**, cioè una breve funzione che collega il punto di intercettazione al codice del monitor.

Il pre-handler della Kprobe si limita alla redirezione, perché non può sospendere il thread nel proprio contesto di esecuzione. Il trampoline chiama quindi un helper C, che applica il controllo tramite il motore di throttling, conservando le informazioni necessarie per raggiungere successivamente la syscall originale.

L'helper chiama il motore, che verifica la selezione della chiamata e la disponibilità di quota tramite l'accounting. Se necessario, il thread attende nella wait queue del motore, risvegliato dal timer o dagli altri eventi previsti. Le regole di selezione, rinnovo e uscita dall'attesa sono descritte nella sezione [Semantica del throttling](#semantica-del-throttling).

Quando la chiamata può procedere, l'helper restituisce al trampoline l'indirizzo della funzione originale di Linux. Il trampoline ripristina il contesto necessario e **salta alla funzione originale senza lasciare un indirizzo di ritorno nel modulo**. Il lavoro del monitor termina quindi prima dell'esecuzione della syscall, anche se questa successivamente si blocca o non ritorna.

Se l'attesa viene interrotta con un errore, il trampoline restituisce invece tale errore al percorso kernel chiamante. Il componente delle statistiche registra l'ingresso e l'uscita dall'attesa del monitor.

### File coinvolti nel flusso

La configurazione e l'esecuzione delle syscall sono due percorsi distinti: il controller configura il modulo, mentre le chiamate dei programmi vengono controllate attraverso il percorso di intercettazione.

| Fase | File principali | Ruolo |
|---|---|---|
| Configurazione | `user/main.c`, `user/controller.c` | Avviano il controller, interpretano i comandi e inviano le richieste `ioctl`. |
| Gestione del device | `kernel/syscall_throttle_main.c` | Registra il device e smista le richieste ai gestori della configurazione. |
| Configurazione e registri | `kernel/config.c`, `kernel/uid_registry.c`, `kernel/program_registry.c`, `kernel/syscall_registry.c` | Gestiscono MAX, ON/OFF e gli elementi registrati. |
| Intercettazione | `kernel/dispatcher_hook.c` | Installa la Kprobe, redirige al trampoline e contiene l'helper C che prepara il passaggio alla syscall originale. |
| Collegamento assembly | `kernel/dispatcher_entry.S` | Conserva il contesto necessario, chiama l'helper C e usa il risultato per saltare alla syscall originale o restituire un errore. |
| Controllo e attesa | `kernel/throttle_engine.c`, `kernel/accounting.c` | Verificano il matching, prenotano quota e gestiscono timer, sospensione e risveglio. |
| Statistiche | `kernel/statistics.c` | Misura waiter e ritardi; il controller calcola la media e stampa i risultati. |

## Requisiti

Per compilare ed eseguire il progetto sono necessari:

- ambiente Linux x86-64;
- toolchain C con `gcc` e `make`;
- header del kernel corrispondenti al kernel in esecuzione, disponibili tramite `/lib/modules/$(uname -r)/build`;
- privilegi di root per caricare/scaricare il modulo e per modificare la configurazione del monitor;
- supporto Kprobes e disponibilità dei simboli kernel `x64_sys_call`, `sys_call_table` e `kallsyms_lookup_name`;
- `CONFIG_FRAME_POINTER=y`, richiesto dall’attuale gestione dello stack nel trampoline assembly.

I programmi controllati e le operazioni di consultazione non richiedono privilegi di root. La compatibilità del modulo è stata verificata nell’ambiente indicato sotto; l’intercettazione utilizza funzioni interne del kernel.

## Ambiente di sviluppo e validazione

Il progetto è stato sviluppato e testato nel seguente ambiente:

- **Distribuzione:** Ubuntu 25.10 (Questing Quokka)
- **Kernel:** Linux 6.17.0-41-generic
- **Architettura:** x86-64
- **Compilatore:** GCC 15.2.0
- **Frame pointer:** `CONFIG_FRAME_POINTER=y`

## Compilazione

Per compilare sia il modulo kernel sia il controller user-space:

```bash
make
```

Il modulo viene generato nella directory `kernel/`, mentre il controller user-space viene generato nella directory `build/` con il nome:

```text
build/syscall_throttle_ctl
```

Per ricompilare il progetto da zero:

```bash
make clean
make
```

È disponibile anche il target `make rebuild`.

`make`, `make clean` e `make rebuild` operano sui file di compilazione: non scaricano né sostituiscono un modulo già caricato nel kernel. Dopo una modifica al codice del modulo, usare `make reload` per eseguire la nuova versione.

Per rimuovere gli artefatti generati:

```bash
make clean
```

## Gestione del modulo

`make` compila il progetto, ma non carica il modulo nel kernel. Per inviare direttamente i comandi del controller descritti sotto, caricare prima il modulo con `make load`, se non è già caricato. Il caricamento rende disponibile `/dev/syscall_throttle` e installa l'intercettazione; la configurazione e l'attivazione del monitor avvengono poi tramite il controller.

Se si eseguono le demo, non occorre lanciare prima `make load`: gli script eseguono già `make reload` e i comandi di configurazione necessari.

| Comando | Operazione |
|---|---|
| `make load` | Compila e carica `kernel/syscall_throttle.ko`. |
| `make unload` | Scarica il modulo dal kernel. |
| `make reload` | Scarica e ricarica il modulo, azzerando configurazione e statistiche. |
| `make logs` | Mostra i messaggi recenti del modulo nel log del kernel. |

## Utilizzo del controller

Il controller user-space viene compilato come:

```text
build/syscall_throttle_ctl
```

Può essere invocato direttamente, ad esempio:

```bash
./build/syscall_throttle_ctl ping
```

oppure tramite il target `controller` del `Makefile`:

```bash
make controller ARGS="ping"
```

I comandi disponibili sono:

```text
ping
get-max
set-max NUMERO

monitor-on
monitor-off
monitor-status

uid-add UID
uid-remove UID
uid-list

program-add NOME
program-remove NOME
program-list

syscall-add NUMERO
syscall-remove NUMERO
syscall-list

stats
```

### Configurazione di MAX

Il valore corrente di `MAX` può essere letto con:

```bash
./build/syscall_throttle_ctl get-max
```

e modificato con:

```bash
sudo ./build/syscall_throttle_ctl set-max 10
```

### Attivazione del monitor

Il monitor può essere attivato, disattivato e interrogato con:

```bash
sudo ./build/syscall_throttle_ctl monitor-on
sudo ./build/syscall_throttle_ctl monitor-off
./build/syscall_throttle_ctl monitor-status
```

### Registrazione degli elementi controllati

La configurazione è mantenuta in memoria dal modulo caricato nel kernel: i comandi del controller non modificano un file di configurazione.

Il registro dei programmi confronta il nome del task Linux (`comm`), ottenuto con `get_task_comm()`. L’interfaccia accetta da 1 a 15 caratteri significativi più il terminatore nullo. Il nome è modificabile e può differire tra thread dello stesso processo.

È possibile registrare fino a 64 UID e 64 nomi di programmi. I numeri delle system call si riferiscono alla ABI x86-64 nativa.

Esempi di registrazione:

```bash
sudo ./build/syscall_throttle_ctl uid-add 1001
sudo ./build/syscall_throttle_ctl program-add my-program
sudo ./build/syscall_throttle_ctl syscall-add 39
```

Gli elementi registrati possono essere visualizzati con:

```bash
./build/syscall_throttle_ctl uid-list
./build/syscall_throttle_ctl program-list
./build/syscall_throttle_ctl syscall-list
```

e rimossi con i corrispondenti comandi `uid-remove`, `program-remove` e `syscall-remove`.

## Semantica del throttling

Una system call viene controllata dal monitor solamente quando valgono entrambe le seguenti condizioni:

1. il numero della system call è registrato;
2. il nome del programma oppure l’effective UID del processo chiamante è registrato.

In forma compatta:

```text
syscall registrata && (programma registrato || EUID registrato)
```

Il limite `MAX` è globale: rappresenta il numero massimo complessivo di system call controllate che possono essere ammesse all’esecuzione durante una finestra temporale di un secondo.

Le finestre temporali sono consecutive, non sovrapposte e gestite autonomamente dal kernel tramite un timer monotono. La prima finestra parte all’attivazione del monitor. All’inizio di una nuova finestra il conteggio delle invocazioni viene azzerato e i thread eventualmente in attesa vengono risvegliati.

Il budget è condiviso tra tutti i programmi, EUID e numeri di syscall selezionati. Il limite riguarda le ammissioni alla syscall reale, non il numero di syscall contemporaneamente in esecuzione. Una syscall bloccante può quindi continuare ad attendere dati dopo essere stata ammessa.

Il controllo non usa una finestra scorrevole: due chiamate possono essere ammesse a breve distanza se appartengono a finestre consecutive.

Quando `MAX` viene raggiunto, il thread che invoca un’altra syscall controllata viene temporaneamente sospeso prima di raggiungere la funzione originale.

Al risveglio per una nuova finestra, se l’attesa non è stata annullata, il thread rivaluta la configurazione corrente e compete nuovamente per la quota. In particolare vengono controllati:

- stato del monitor;
- registrazione della system call;
- registrazione del programma;
- effective UID corrente;
- disponibilità rispetto al valore di `MAX`.

Se la chiamata non risulta più soggetta al controllo, il thread può procedere senza consumare quota. OFF e shutdown annullano invece l’attesa e consentono la prosecuzione senza richiedere un posto nella finestra; un segnale può interrompere l’attesa con un errore.

Quando il monitor è disattivato, nessun limite viene applicato alle system call e le invocazioni procedono normalmente. `monitor-off` risveglia i waiter senza attendere il tick successivo.

`monitor-on` è idempotente: se il monitor è già attivo, non azzera la quota consumata e non riavvia il timer. Dopo una disattivazione, una nuova attivazione apre una finestra con quota vuota.

Le modifiche di MAX e dei registri non provocano da sole un risveglio immediato: vengono considerate alla successiva rivalutazione.

Non viene imposto un ordine FIFO tra i thread in attesa: dopo il risveglio per una nuova finestra i thread competono nuovamente per la possibilità di eseguire la system call.

## Demo

Il repository include alcune demo pensate per mostrare in modo diretto e leggibile i principali comportamenti del monitor.

Prima delle demo, compilare il progetto dalla sua cartella principale:

```bash
make
```

Le demo compilano il relativo programma user-space, scaricano e ricaricano il modulo per partire da uno stato iniziale, configurano il caso di interesse e disattivano il monitor al termine. Le registrazioni aggiunte vengono rimosse dagli script. La configurazione precedente al ricaricamento del modulo non viene conservata.

Le demo possono essere avviate tramite i corrispondenti target del `Makefile`. Per la demo 5 usare una sessione non-root, poiché viene confrontata la modifica senza privilegi con quella eseguita tramite `sudo`.

### Demo 1 — Matching tramite nome del programma

```bash
make demo1
```

Configura `MAX=1`, registra il programma `demo_prog` e la system call `getpid`.

Il programma esegue più invocazioni consecutive di `getpid()`. La prima può procedere immediatamente, mentre le successive vengono ritardate fino all'apertura delle finestre temporali successive.

La demo mostra quindi il matching basato sul nome del task Linux (`comm`).

Infine rimuove `demo_prog` dal registro lasciando il monitor acceso e riesegue il programma: le quattro chiamate devono procedere senza attesa del monitor. Questa fase verifica l'effetto della deregistrazione del nome.

### Demo 2 — Matching tramite EUID

```bash
make demo2
```

Verifica il secondo ramo della condizione di matching utilizzando un EUID registrato e lasciando vuoto il registro dei programmi.

La demo utilizza l'account dedicato `throttle_demo`, creandolo se necessario, e confronta l'esecuzione dello stesso programma con:

- EUID registrato, soggetto al throttling;
- EUID non registrato, non soggetto al throttling.

In questo modo viene mostrato che programma ed EUID costituiscono criteri alternativi di selezione.

### Demo 3 — Concorrenza e statistiche

```bash
make demo3
```

Avvia tre worker concorrenti che eseguono `getpid()` con `MAX=1`.

Poiché il limite è globale, una sola system call controllata può utilizzare il budget disponibile in ciascuna finestra. I worker completano quindi su finestre temporali differenti.

Al termine vengono mostrate anche le statistiche del monitor, tra cui:

- picco dei thread contemporaneamente bloccati;
- media dei thread bloccati;
- massimo ritardo osservato;
- UID associato al massimo ritardo;
- programma associato al massimo ritardo.

Segue una verifica aggiuntiva con due istanze di `demo_prog`, ciascuna con quattro `getpid`. Il modulo viene ricaricato per separare le statistiche dalla prova dei worker. Con `MAX=1` le otto chiamate condividono la stessa quota anche fra processi distinti: il completamento complessivo è atteso circa sette secondi dopo la prima ammissione. I log mostrano PID diversi; l'ordine di ammissione non è garantito e i tempi stampati sono relativi all'avvio di ciascun processo. Questa fase aggiunge circa sette secondi alla demo e riutilizza il programma esistente.

I due processi non sono obbligati ad alternarsi: uno può ottenere quota in più finestre consecutive, mentre l'altro continua ad aspettare. Gli output vengono salvati separatamente e mostrati solo dopo che entrambi sono terminati e il monitor è stato spento. Per questo nel terminale `Monitor disattivato` compare prima dei log dei processi, anche se le loro chiamate sono state eseguite durante il periodo ON.

### Demo 4 — Rivalidazione dopo `monitor-off`

```bash
make demo4
```

Configura il monitor in modo da ottenere almeno un thread realmente bloccato e verifica tramite le statistiche che sia presente un waiter.

Il monitor viene quindi disattivato mentre il thread è in attesa.

Il waiter viene risvegliato, riconosce la disattivazione del monitor e procede senza richiedere quota, senza attendere la finestra temporale successiva.

### Demo 5 — Privilegi

```bash
make demo5
```

Mostra la distinzione tra operazioni di consultazione e operazioni di modifica della configurazione.

I comandi read-only, come `get-max`, `monitor-status` e `stats`, vengono eseguiti senza privilegi di root.

Una modifica della configurazione eseguita da un utente non privilegiato viene invece rifiutata, mentre la stessa operazione eseguita con effective UID pari a `0` viene accettata.

### Demo 6 — System call bloccante: `read` su pipe

```bash
make demo6
```

Configura `MAX=1`, registra la syscall `read` (numero 0 su x86-64) e il nome del reader `soa-read-demo`. Il programma `demo_blocking.c` esegue due letture di un byte dalla stessa pipe. Un writer fornisce i due byte dopo circa 200 ms.

Il confronto tra monitor OFF e ON distingue l’attesa dei dati dall’attesa imposta dal monitor:

| Operazione | Monitor OFF | Monitor ON |
|---|---|---|
| Prima `read`, pipe inizialmente vuota | Attesa dei dati, circa 200 ms | Attesa dei dati, circa 200 ms |
| Seconda `read`, con un byte già disponibile | Completamento immediato | Attesa della nuova finestra, circa altri 800 ms |
| Picco dei waiter del monitor | 0 | 1 |

Prima della seconda lettura il programma mostra che nella pipe rimane un byte. Il ritardo della seconda `read` con monitor ON è quindi dovuto alla quota esaurita. Il writer consulta anche il numero dei waiter: l’attesa della prima lettura per i dati non viene conteggiata come blocking del monitor.

I tempi sono indicativi e dipendono dalla fase della finestra e dallo scheduling. Nell’esecuzione osservata, la seconda lettura con monitor ON ha impiegato circa 794 ms e il ritardo massimo del monitor è risultato coerente con tale attesa.

### Esecuzione di tutte le demo

Le sei demo possono essere eseguite in sequenza con:

```bash
make demo-all
```

Poiché le demo caricano il modulo kernel e modificano temporaneamente la configurazione del monitor, durante l'esecuzione può essere richiesta l'autenticazione tramite `sudo`.

## Statistiche

Il modulo mantiene statistiche globali relative al comportamento del monitor.

Il comando:

```bash
./build/syscall_throttle_ctl stats
```

permette di visualizzare:

- numero corrente di thread bloccati;
- numero massimo di thread contemporaneamente bloccati;
- numero medio di thread bloccati durante il tempo di attività del monitor;
- tempo complessivo durante il quale il monitor è rimasto attivo;
- massimo ritardo osservato prima dell’esecuzione effettiva di una system call;
- UID associato al massimo ritardo;
- nome del programma associato al massimo ritardo.

Il numero medio di thread bloccati è una media temporale, calcolata rispetto ai soli intervalli durante i quali il monitor è attivo:

```text
media = weighted_blocking_time_ns / monitor_enabled_time_ns
```

Il numeratore è l’integrale temporale del numero di thread in attesa nel monitor. Gli intervalli con monitor attivo e nessun waiter contribuiscono al denominatore con contributo nullo al numeratore.

Il ritardo massimo misura il tempo tra l’ingresso nello stato di attesa del monitor e la sua uscita prima di proseguire verso la syscall reale. Non include l’eventuale attesa dei dati durante l’esecuzione della syscall. Il picco viene aggiornato per le attese concluse con prosecuzione verso la syscall; se non ne è stata completata alcuna, viene mostrato «non disponibile».

Le statistiche sono cumulative dalla vita del modulo: `monitor-off` non le azzera e una modifica di MAX non apre una nuova raccolta statistica. Scaricare e ricaricare il modulo permette di iniziare una nuova osservazione.

## Avvertenze operative

Il throttling viene applicato realmente ai thread le cui invocazioni soddisfano la configurazione del monitor. Di conseguenza, una configurazione particolarmente restrittiva può avere effetti visibili sul comportamento dei processi interessati.

È necessario prestare particolare attenzione quando viene registrato un EUID. In questo caso il controllo non riguarda un singolo programma, ma potenzialmente tutti i processi eseguiti con quell’effective UID che invocano una delle system call registrate.

Poiché `MAX` è un limite globale condiviso tra tutte le invocazioni controllate, registrare l’EUID della propria sessione utente insieme a una system call molto frequente e impostare un valore di `MAX` molto basso, ad esempio `MAX=1`, può causare il blocco temporaneo di numerosi processi contemporaneamente. In un ambiente desktop questo può rendere l’interfaccia grafica fortemente rallentata o temporaneamente non responsiva.

Per questo motivo, le prove basate sul matching tramite EUID dovrebbero essere eseguite preferibilmente utilizzando un account dedicato, un UID isolato oppure una macchina virtuale.

Questo comportamento è una conseguenza diretta della semantica del monitor e non indica un malfunzionamento del modulo: tutti i processi che soddisfano i criteri configurati competono per lo stesso budget globale di system call disponibile nella finestra temporale corrente.
