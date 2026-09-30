# Syscall Throttling - Linux Kernel Module

Progetto sviluppato per il corso di **Advanced Operating Systems (and System Security)**, A.A. 2025-2026, Università degli Studi di Roma Tor Vergata.

Il progetto implementa un **Linux Kernel Module (LKM)** per il throttling delle system call su architettura x86-64.

Il modulo permette di registrare nomi di programmi, effective user ID (EUID) e numeri di system call. Una system call viene controllata quando il suo numero è registrato e il processo chiamante corrisponde a un programma registrato oppure a un EUID registrato.

La condizione di controllo è quindi:

```text
syscall registrata && (programma registrato || EUID registrato)
```

Il monitor applica un limite globale `MAX` al numero di system call controllate che possono essere eseguite in ogni finestra temporale di un secondo. Quando il limite viene raggiunto, le ulteriori invocazioni vengono temporaneamente bloccate.

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
- suite automatica di test user-space.
- demo riproducibili dei principali comportamenti del monitor.

## Struttura del repository

```text
.
├── Makefile
├── demos/
│   ├── demo_prog.c
│   ├── demo_concurrency.c
│   ├── run_demo1.sh
│   ├── run_demo2.sh
│   ├── run_demo3.sh
│   ├── run_demo4.sh
│   └── run_demo5.sh
├── include/
│   └── syscall_throttle_ioctl.h
├── kernel/
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
├── user/
│   ├── controller.c
│   ├── controller.h
│   └── main.c
└── tests/
    ├── include/
    │   └── test_common.h
    ├── lib/
    │   └── common.sh
    ├── run_test.sh
    ├── test_common.c
    ├── test_control_plane.c
    ├── test_registries.c
    ├── test_throttling.c
    ├── test_concurrency.c
    └── test_revalidation.c
```

Le directory principali hanno i seguenti ruoli:

- `include/`: interfaccia condivisa tra kernel-space e user-space, incluse strutture dati e richieste `ioctl`;
- `kernel/`: implementazione del modulo, dei registri, del monitor, dell’intercettazione delle system call e delle statistiche;
- `user/`: controller user-space utilizzato per configurare e interrogare il modulo;
- `demos/`: programmi e script utilizzati per le dimostrazioni interattive del comportamento del monitor;
- `tests/`: infrastruttura e test automatici del comportamento del sistema.

## Requisiti

Per compilare ed eseguire il progetto sono necessari:

- ambiente Linux x86-64;
- toolchain C con `gcc` e `make`;
- header del kernel corrispondenti al kernel in esecuzione, disponibili tramite `/lib/modules/$(uname -r)/build`;
- privilegi di root per caricare/scaricare il modulo e per modificare la configurazione del monitor.


## Ambiente di sviluppo e test

Il progetto è stato sviluppato e testato nel seguente ambiente:

- **Distribuzione:** Ubuntu 25.10 (Questing Quokka)
- **Kernel:** Linux 6.17.0-41-generic
- **Architettura:** x86-64
- **Compilatore:** GCC 15.2.0



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
make rebuild
```

Per rimuovere gli artefatti generati:

```bash
make clean
```

## Caricamento e scaricamento del modulo

Il modulo può essere caricato con:

```bash
make load
```

e scaricato con:

```bash
make unload
```

Per scaricare e ricaricare il modulo in un unico comando:

```bash
make reload
```

I messaggi recenti prodotti dal modulo nel kernel log possono essere visualizzati con:

```bash
make logs
```

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

Il limite `MAX` è globale: rappresenta il numero massimo complessivo di system call controllate che possono essere eseguite durante una finestra temporale di un secondo.

Le finestre temporali sono consecutive, non sovrapposte e gestite autonomamente dal kernel tramite un timer. All’inizio di una nuova finestra il conteggio delle invocazioni viene azzerato e i thread eventualmente in attesa vengono risvegliati.

Quando `MAX` viene raggiunto, le ulteriori invocazioni controllate non eseguono immediatamente la system call reale, ma il thread chiamante viene temporaneamente sospeso.

Al risveglio, il thread rivaluta completamente la configurazione corrente. In particolare vengono nuovamente controllati:

- stato del monitor;
- registrazione della system call;
- registrazione del programma;
- effective UID corrente;
- disponibilità rispetto al valore di `MAX`.

Questo permette, ad esempio, a un thread già bloccato di procedere se nel frattempo il monitor viene disattivato o la sua invocazione non risulta più soggetta al controllo.

Quando il monitor è disattivato, nessun limite viene applicato alle system call e le invocazioni procedono normalmente.

Non viene imposto un ordine FIFO tra i thread in attesa: dopo ogni risveglio i thread competono nuovamente per la possibilità di eseguire la system call.

## Test automatici

Il repository include una suite di test user-space eseguibile con:

```bash
make test
```

La suite compila il modulo e i programmi di test, carica e scarica automaticamente il modulo per ogni caso e controlla la presenza di errori critici nel kernel log.

I test disponibili sono:

- `test-control-plane`: verifica apertura del device, `PING`, lettura di `MAX` e attivazione/disattivazione del monitor;
- `test-registries`: verifica registrazione, consultazione, duplicati e deregistrazione di UID, programmi e system call;
- `test-throttling`: con `MAX=1` verifica che la seconda system call controllata venga posticipata alla finestra successiva;
- `test-concurrency`: verifica il limite globale con più thread concorrenti e `MAX=1`;
- `test-revalidation`: verifica che un thread bloccato venga rivalidato quando il monitor viene disattivato.

Ogni test può essere eseguito anche singolarmente:

```bash
make test-control-plane
make test-registries
make test-throttling
make test-concurrency
make test-revalidation
```

Con `MAX=1`, il test di throttling osserva un ritardo di circa un secondo sulla seconda invocazione, mentre il test concorrente distribuisce i completamenti su finestre consecutive.

Il test di rivalidazione verifica inoltre che `monitor-off` risvegli immediatamente un waiter senza attendere la finestra successiva.

Al termine di ogni test viene verificato che il modulo sia scaricato e che non siano comparsi errori kernel critici.


## Demo

Il repository include alcune demo pensate per mostrare in modo diretto e leggibile i principali comportamenti del monitor.

Le demo configurano automaticamente il modulo, eseguono il caso di interesse e ripristinano la configurazione al termine. Possono essere avviate tramite i corrispondenti target del `Makefile`.

### Demo 1 — Matching tramite nome del programma

```bash
make demo1
```

Configura `MAX=1`, registra il programma `demo_prog` e la system call `getpid`.

Il programma esegue più invocazioni consecutive di `getpid()`. La prima può procedere immediatamente, mentre le successive vengono ritardate fino all'apertura delle finestre temporali successive.

La demo mostra quindi il matching basato sul nome del task Linux (`comm`).

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

### Demo 4 — Rivalidazione dopo `monitor-off`

```bash
make demo4
```

Configura il monitor in modo da ottenere almeno un thread realmente bloccato e verifica tramite le statistiche che sia presente un waiter.

Il monitor viene quindi disattivato mentre il thread è in attesa.

Il waiter viene risvegliato, rivalida la configurazione corrente e può procedere senza attendere l'apertura della finestra temporale successiva.

### Demo 5 — Privilegi

```bash
make demo5
```

Mostra la distinzione tra operazioni di consultazione e operazioni di modifica della configurazione.

I comandi read-only, come `get-max`, `monitor-status` e `stats`, vengono eseguiti senza privilegi di root.

Una modifica della configurazione eseguita da un utente non privilegiato viene invece rifiutata, mentre la stessa operazione eseguita con effective UID pari a `0` viene accettata.

### Esecuzione di tutte le demo

Le cinque demo possono essere eseguite in sequenza con:

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

Il numero medio di thread bloccati è calcolato rispetto al solo intervallo temporale durante il quale il monitor è attivo.

## Avvertenze operative

Il throttling viene applicato realmente ai thread le cui invocazioni soddisfano la configurazione del monitor. Di conseguenza, una configurazione particolarmente restrittiva può avere effetti visibili sul comportamento dei processi interessati.

È necessario prestare particolare attenzione quando viene registrato un EUID. In questo caso il controllo non riguarda un singolo programma, ma potenzialmente tutti i processi eseguiti con quell’effective UID che invocano una delle system call registrate.

Poiché `MAX` è un limite globale condiviso tra tutte le invocazioni controllate, registrare l’EUID della propria sessione utente insieme a una system call molto frequente e impostare un valore di `MAX` molto basso, ad esempio `MAX=1`, può causare il blocco temporaneo di numerosi processi contemporaneamente. In un ambiente desktop questo può rendere l’interfaccia grafica fortemente rallentata o temporaneamente non responsiva.

Per questo motivo, le prove basate sul matching tramite EUID dovrebbero essere eseguite preferibilmente utilizzando un account dedicato, un UID isolato oppure una macchina virtuale.

Questo comportamento è una conseguenza diretta della semantica del monitor e non indica un malfunzionamento del modulo: tutti i processi che soddisfano i criteri configurati competono per lo stesso budget globale di system call disponibile nella finestra temporale corrente.
