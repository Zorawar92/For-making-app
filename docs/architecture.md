# Architettura: port nativo webOS, con Android runtime ristretto

## Decisione

Costruire una **nuova app webOS**, non un contenitore grafico Android. Waydroid è utile per studiare come assemblare ed eseguire un ambiente Android su Linux, ma non è un formato di portabilità per webOS e non produce IPK installabili su webOS.

Waydroid dichiara di avviare un sistema Android completo in un container e dipende da Linux namespaces (user, pid, uts, net, mount, ipc), da un'immagine Android/LineageOS e dall'accesso alle risorse hardware. Il target dichiarato è una TV INNO-Hit con **webOS Hub**, non hardware LG: kernel, SoC, firmware e policy possono differire sia tra modelli OEM sia dal webOS TV di riferimento. Non si deve assumere la compatibilità degli IPK per LG, né la disponibilità di LXC, namespace, driver Binder o privilegi necessari ad avviare quel container. Inserire l'intera immagine Waydroid nell'IPK sarebbe inoltre molto pesante e non renderebbe automaticamente nativa l'interazione con telecomando, video e storage.

## Cosa riutilizzare dai progetti di riferimento

### CSWeb TV

CSWeb TV è la prova di fattibilità e il riferimento implementativo principale. Il suo modello architetturale è già ibrido, non una conversione APK:

1. app e interfaccia televisiva native;
2. runtime ART avviato come processo separato per il bytecode dei plugin `.cs3`;
3. API/compatibilità Android limitata alle chiamate che i provider supportati usano;
4. scambio richieste/risultati attraverso un protocollo locale;
5. playback attraverso la pipeline media nativa webOS.

Per un nuovo port useremo questa architettura come benchmark, riutilizzando codice solo se serve e rispettando licenze, avvisi e attribuzione. Prima di reinventare ART, plugin loading e player è necessario confrontare lo scope con la release e il codice aggiornati di CSWeb TV.

### Waydroid

Dallo studio di Waydroid si possono prendere idee relative a:

- processo/runtime separato dal frontend;
- root filesystem e dipendenze versionati;
- controllo esplicito di architettura, ABI e compatibilità kernel;
- diagnostica e test riproducibili dell'ambiente Android.

Non si assume invece di poter portare su webOS l'intero container Waydroid, il suo system image o le sue dipendenze Linux. Se un test sul dispositivo dimostrasse che una parte è indispensabile, va valutata separatamente e non deve diventare una dipendenza silenziosa dell'IPK.

## Componenti previsti

```text
IPK webOS
├── UI nativa TV (telecomando, catalogo, ricerca, dettagli)
├── gestore repository/plugin (.cs3), con doGior come repo opt-in
├── client IPC locale con timeout, limiti messaggi e log senza token
├── player webOS e adattatore link/sottotitoli
└── runtime helper locale
    ├── ART/compatibilità Android minima, solo API necessarie ai provider supportati
    ├── API CloudStream/JVM bloccate a revisioni e hash noti
    └── protocollo strutturato per ricerca, dettagli, episodi e stream
```

Il browser/HTML5 può essere usato solo se necessario per un elemento UI isolato; non è l'obiettivo di esecuzione per il plugin Android. Il percorso primario è app webOS nativa con toolchain e packaging webOS.

## Confini di sicurezza e compatibilità

I plugin sono codice di terze parti con accesso di rete. Il runtime helper dovrebbe essere un processo distinto per robustezza, ma questo **non equivale** a una sandbox di sicurezza: su webOS i processi dell'app possono condividere lo stesso utente e privilegi. Non distribuire credenziali o segreti nel pacchetto. Limitare dimensione dei plugin, verificare hash dove disponibile, evitare log di URL con token e rendere visibili all'utente le fonti aggiunte.

La compatibilità va dichiarata per combinazione di:

- modello TV, versione webOS/firmware, architettura kernel e memoria disponibile;
- ABI del runtime e librerie effettivamente presenti nel firmware;
- API Android/CloudStream richieste dal singolo plugin;
- protocolli, codec, DRM, header, TLS e limiti del player nativo.

Non promettere compatibilità generale con tutti i provider Cloudstream. In particolare, WebView, UI Android/AndroidX, DRM Widevine/PlayReady, download e plugin che dipendono da servizi Google richiedono supporto dedicato o possono rimanere esclusi.

## Fonte doGior

Il manifest remoto da offrire nella UI (senza congelare l'elenco dei plugin nell'IPK) è:

```text
https://raw.githubusercontent.com/doGior/doGiorsHadEnough/builds/repo.json
```

La prima milestone dovrebbe consentire di aggiungere la repo manualmente, leggere il manifest, mostrare l'elenco e installare un plugin campione solo dopo conferma. I plugin vanno caricati dal ramo `builds` aggiornato dal mantenitore; non committare `.cs3` scaricati nel progetto.

## Riferimenti primari

- [Waydroid README](https://github.com/waydroid/waydroid): container Android completo e requisiti Linux.
- [CSWeb TV README](https://github.com/sostk/cloudstream-webos-native): architettura, pacchetti, limiti e guida di build.
- [Decisione di fattibilità CSWeb TV](https://github.com/sostk/cloudstream-webos-native/blob/main/docs/FEASIBILITY_DECISION.md): esempi di plugin provati, limiti e risorse misurate.
- [doGior manifest](https://raw.githubusercontent.com/doGior/doGiorsHadEnough/builds/repo.json).
