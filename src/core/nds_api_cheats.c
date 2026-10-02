#include <stdint.h>
#include <stddef.h>
#include "hires_runtime.h"
#include "blob_symbols.h"
#include "seedlessds/nds.h"
#include "core/nds_state.h"
#include "frontend/frontend.h"
#include "frontend/cheats/cheats.h"
#include "core_internals.h"


static cheats_t *cheats_of(void)
{
    nds_t *machine = FRONTEND->machine;

    if (machine == NULL)
        return NULL;
    return &machine->cart.cheats;
}

static cheats_t *cheats_loaded(void)
{
    cheats_t *cheats = cheats_of();

    if (cheats == NULL)
        return NULL;
    if (cheats->index_loaded == 0 || cheats->database_loaded == 0)
        return NULL;
    return cheats;
}

int nds_cheat_count(nds_t *nds)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return (int)cheats->entry_count;
}

int nds_cheat_enabled(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return cheats->entries[index].enabled[0] != 0;
}

void nds_cheat_set_enabled(nds_t *nds, int index, int enabled)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return;
    cheats->entries[index].enabled[0] = (uint8_t)(((uint32_t)enabled & 0xffu) != 0);
}

const char *nds_cheat_name(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return NULL;
    return cheats->entries[index].name;
}

const char *nds_cheat_note(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return NULL;
    return cheats->entries[index].description;
}

int nds_cheat_folder_id(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return (int)cheats->entries[index].folder;
}

int nds_cheat_folder_count(nds_t *nds)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return (int)cheats->folder_count;
}

const char *nds_cheat_folder_name(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return NULL;
    return cheats->folders[index].name;
}

const char *nds_cheat_folder_note(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return NULL;
    return cheats->folders[index].description;
}

int nds_cheat_folder_expanded(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return cheats->folders[index].enabled[0] != 0;
}

void nds_cheat_set_folder_expanded(nds_t *nds, int index, int expanded)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return;
    cheats->folders[index].enabled[0] = (uint8_t)(((uint32_t)expanded & 0xffu) != 0);
}

int nds_cheat_folder_multi_select(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return cheats->folders[index].type != CHEAT_FOLDER_EXCLUSIVE;
}

int nds_cheat_custom_count(nds_t *nds)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return (int)cheats->user_count;
}

const char *nds_cheat_custom_name(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL || cheats->user_entries == NULL)
        return NULL;
    return cheats->user_entries[index].name;
}

const int32_t *nds_cheat_custom_data(nds_t *nds, int index, uint32_t *words)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL || cheats->user_entries == NULL) {
        *words = 0;
        return NULL;
    }
    *words = cheats->user_entries[index].code_words;
    return cheats->user_entries[index].codes;
}

int nds_cheat_custom_enabled(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL || cheats->user_entries == NULL)
        return 0;
    return cheats->user_entries[index].enabled[0] != 0;
}

void nds_cheat_set_custom_enabled(nds_t *nds, int index, int enabled)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL || cheats->user_entries == NULL)
        return;
    cheats->user_entries[index].enabled[0] = (uint8_t)(((uint32_t)enabled & 0xffu) != 0);
    if (cheats_write_user_file(cheats, FRONTEND->machine) == 0)
        cheats_rebuild_active_list(cheats);
}

int nds_cheat_add_custom(nds_t *nds, const char *name, const int32_t *codes,
                         uint32_t words, int enabled)
{
    cheats_t *cheats = cheats_of();
    cheat_entry_t *entry;

    (void)nds;
    if (cheats == NULL)
        return NDS_CHEAT_ADD_FAILED;

    if (cheats_list_add_entry((unsigned char *)(cheats), name, codes, words) != 0)
        return NDS_CHEAT_ADD_FAILED;

    if (cheats->user_entries == NULL)
        return NDS_CHEAT_ADD_FAILED;

    entry = &cheats->user_entries[cheats->user_count - 1u];
    entry->enabled[0] = (uint8_t)((((uint32_t)enabled) & 0xffu) != 0);

    if (cheats_write_user_file(cheats, FRONTEND->machine) != 0)
        return NDS_CHEAT_WRITE_FAILED;

    cheats_rebuild_active_list(cheats);
    return NDS_CHEAT_ADDED;
}

int nds_cheat_find_custom(nds_t *nds, const int32_t *codes, uint32_t words)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL)
        return 0;
    return (int)cheats_find_interned_entry(cheats, codes, words);
}

void nds_cheat_remove_custom(nds_t *nds, int index)
{
    cheats_t *cheats = cheats_of();

    (void)nds;
    if (cheats == NULL || cheats->user_entries == NULL)
        return;

    cheats_remove_entry(cheats, (uint32_t)index);

    if (cheats_write_user_file(cheats, FRONTEND->machine) != 0)
        return;

    cheats_rebuild_active_list(cheats);
}

void nds_cheat_update(nds_t *nds, int enabled)
{
    cheats_t *cheats = cheats_loaded();

    (void)nds;
    if (cheats == NULL)
        return;

    if (cheats_save_record(cheats) != 0)
        return;

    FRONTEND->cheats_update_request = (uint8_t)(((uint32_t)enabled & 0xffu) != 0);
}
