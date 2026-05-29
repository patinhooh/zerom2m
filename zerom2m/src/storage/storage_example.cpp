/*
 * storage_example.cpp
 *
 * Exemplo de integração do StorageManager com o HTTP handler do ZeroM2M.
 * Mostra os padrões típicos para POST e GET de ContentInstances,
 * e Write/Read de recursos mutáveis (AE, Container, Subscription).
 */
#include <zerom2m/storage/storage.h>

using namespace zerom2m::storage;

// ─── Inicialização (chamar após montar o sistema de ficheiros FAT32) ──────────

void StorageInit()
{
    // "0:" é o drive FAT32 no Circle (configurável em cmdline.txt)
    StorageResult r = StorageManager::Get().Init("0:");
    // r == StorageResult::OK se o SD card estiver acessível

    // Garantir que a estrutura de diretórios existe para um AE/Container
    StorageManager::Get().EnsureContainer("myAE", "readings");
}

// ─── POST /app/myAE/container/readings ───────────────────────────────────────
// Chamado pelo HTTP handler quando recebe um POST de ContentInstance

void HandlePostContent(const char *aeID, const char *cntName,
                       const char *jsonPayload,    // ex: {"value":23.5}
                       u64 timestamp)              // segundos desde epoch
{
    StorageResult r = StorageManager::Get().AppendContent(
        aeID, cntName,
        timestamp,
        jsonPayload,
        static_cast<u32>(strlen(jsonPayload))
    );
    // f_sync é chamado automaticamente dentro de AppendContent

    if (r == StorageResult::OK) {
        // responder 201 Created
    } else {
        // responder 500
    }
}

// ─── GET /app/myAE/container/readings/latest ─────────────────────────────────

void HandleGetLatest(const char *aeID, const char *cntName,
                     char *outBuf, u32 *outLen, u64 *outTimestamp)
{
    StorageResult r = StorageManager::Get().ReadLatest(
        aeID, cntName,
        outBuf, outLen, outTimestamp
    );

    if (r == StorageResult::OK) {
        outBuf[*outLen] = '\0'; // null-terminate para usar como string
        // responder 200 com outBuf
    } else if (r == StorageResult::NotFound) {
        // responder 404
    } else if (r == StorageResult::CorruptRecord) {
        // responder 500 — registo corrompido (power loss parcial)
    }
}

// ─── GET /app/myAE/container/readings/100 ────────────────────────────────────
// Aceder ao 100º ContentInstance (0-based, 0 = o mais antigo)

void HandleGetByIndex(const char *aeID, const char *cntName, u32 index,
                      char *outBuf, u32 *outLen)
{
    StorageResult r = StorageManager::Get().ReadByIndex(
        aeID, cntName, index,
        outBuf, outLen
    );
    // tratar r conforme acima
}

// ─── Recursos mutáveis: AE ───────────────────────────────────────────────────
// Guardar um AE no MetaStore após criação

void StoreAE(const char *resourceID /* ex: "ae-abc123" */)
{
    // Arrays paralelos de chaves e valores
    const char *keys[]   = { "ty",  "ri",        "rn",    "api",     "aei",     "rr"  };
    const char *values[] = { "2",   resourceID,  "myAE",  "N.myApp", "C-12345", "true" };

    StorageResult r = StorageManager::Get().WriteMeta(
        resourceID, keys, values, 6
    );
}

// Ler um AE de volta
void LoadAE(const char *resourceID)
{
    char keys[16][64];
    char values[16][256];
    u32 count = 0;

    StorageResult r = StorageManager::Get().ReadMeta(
        resourceID,
        keys, values, 16, &count
    );

    if (r == StorageResult::OK) {
        for (u32 i = 0; i < count; i++) {
            // keys[i] / values[i]
            // ex: "ty" -> "2", "ri" -> "ae-abc123" ...
        }
    }
}

// ─── Sync periódico (chamar no loop principal ou em timer ISR-safe) ───────────

void PeriodicSync()
{
    // Garante que todos os logs abertos estão persistidos no cartão SD.
    // Chamar pelo menos a cada 10–30 s ou antes de power-off controlado.
    StorageManager::Get().SyncAll();
}

// ─── Padrão de uso no ContainerManager ───────────────────────────────────────
//
//  HTTP Server
//      │
//      ▼
//  ContainerManager::HandleRequest(req)
//      │
//      ├─ POST  → EnsureContainer + AppendContent + SyncAll (ou periódico)
//      │
//      └─ GET   → ReadLatest / ReadByIndex
//                     │
//                 StorageManager (singleton)
//                     ├─ LogStore[aeID/cntName]   (data.log, append-only)
//                     └─ MetaStore                (/meta/*.dat, key=value)
//
// Estrutura de ficheiros resultante no cartão SD:
// atualmente:
//  0:/
//  ├── apps/
//  │   ├── myAE/
//  │   │   ├── readings/
//  │   │   │   └── cnt.log   ← registos binários (header + payload + CRC32)
//  │   │   └── telemetry/
//  │   │       └── data.log
//  │   └── sensorAE/
//  │       └── temperature/
//  │           └── data.log
//  └── meta/
//      ├── ae-abc123.dat       ← AE serializado (key=value + CRC32)
//      ├── cnt-xyz789.dat      ← Container serializado
//      └── sub-001.dat         ← Subscription serializada

// novo:
//
//  0:/
//  ├── apps/
//  │   ├── myAE/
//         ae-abc123.dat 
//  │   │   ├── readings/ (CNT)
//               cnt-xyz789.dat      ← Container serializado com a estrutura resources em binario (que é o que a estrutura já está) (header + payload(que contem os campos obrigatorios + os outros da esturutra com 0 ou null) + CRC32)
//               ├── subscription/ (SUB)
//                sub-001.dat         ← Subscription serializada
//               ├── CIN/ (record)
//                sub-001.dat         ← Subscription serializada

