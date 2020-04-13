#include <patch_common/FunHook.h>
#include <patch_common/CodeInjection.h>
#include <common/rfproto.h>
#include "../rf/common.h"
#include "../rf/player.h"
#include "../rf/trigger.h"
#include "../rf/network.h"

constexpr uint8_t TRIGGER_CLIENT_SIDE = 0x2;
constexpr uint8_t TRIGGER_SOLO = 0x4;
constexpr uint8_t TRIGGER_TELEPORT = 0x8;

rf::Player* g_trigger_solo_player = nullptr;

void SendTriggerActivatePacket(rf::Player* player, int trigger_uid, int32_t entity_handle)
{
    rfTriggerActivate packet;
    packet.type = RF_TRIGGER_ACTIVATE;
    packet.size = sizeof(packet) - sizeof(rfPacketHeader);
    packet.uid = trigger_uid;
    packet.entity_handle = entity_handle;
    rf::NwSendReliablePacket(player, reinterpret_cast<uint8_t*>(&packet), sizeof(packet), 0);
}

FunHook<void(int, int)> SendTriggerActivatePacketToAllPlayers_hook{
    0x00483190,
    [](int trigger_uid, int entity_handle) {
        if (g_trigger_solo_player)
            SendTriggerActivatePacket(g_trigger_solo_player, trigger_uid, entity_handle);
        else
            SendTriggerActivatePacketToAllPlayers_hook.CallTarget(trigger_uid, entity_handle);
    },
};

FunHook<void(rf::TriggerObj*, int32_t, bool)> TriggerActivate_hook{
    0x004C0220,
    [](rf::TriggerObj* trigger, int32_t h_entity, bool skip_movers) {
        // Check team
        auto player = rf::GetPlayerFromEntityHandle(h_entity);
        auto trigger_name = trigger->_super.name.CStr();
        if (player && trigger->team != -1 && trigger->team != player->team) {
            // rf::DcPrintf("Trigger team does not match: %d vs %d (%s)", trigger->team, Player->blue_team,
            // trigger_name);
            return;
        }

        // Check if this is Solo or Teleport trigger (REDPF feature)
        uint8_t ext_flags = trigger_name[0] == '\xAB' ? trigger_name[1] : 0;
        bool is_solo_trigger = (ext_flags & (TRIGGER_SOLO | TRIGGER_TELEPORT)) != 0;
        if (rf::is_net_game && rf::is_local_net_game && is_solo_trigger && player) {
            // rf::DcPrintf("Solo/Teleport trigger activated %s", trigger_name);
            if (player != rf::local_player) {
                SendTriggerActivatePacket(player, trigger->_super.uid, h_entity);
                return;
            }
            else {
                g_trigger_solo_player = player;
            }
        }

        // Normal activation
        // rf::DcPrintf("trigger normal activation %s %d", trigger_name, ext_flags);
        TriggerActivate_hook.CallTarget(trigger, h_entity, skip_movers);
        g_trigger_solo_player = nullptr;
    },
};

CodeInjection TriggerCheckActivation_patch{
    0x004BFC7D,
    [](auto& regs) {
        auto trigger = reinterpret_cast<rf::TriggerObj*>(regs.eax);
        auto trigger_name = trigger->_super.name.CStr();
        uint8_t ext_flags = trigger_name[0] == '\xAB' ? trigger_name[1] : 0;
        bool is_client_side = (ext_flags & TRIGGER_CLIENT_SIDE) != 0;
        if (is_client_side)
            regs.eip = 0x004BFCDB;
    },
};

void ApplyTriggerPatches()
{
    // Solo/Teleport triggers handling + filtering by team ID
    TriggerActivate_hook.Install();
    SendTriggerActivatePacketToAllPlayers_hook.Install();

    // Client-side trigger flag handling
    TriggerCheckActivation_patch.Install();
}
