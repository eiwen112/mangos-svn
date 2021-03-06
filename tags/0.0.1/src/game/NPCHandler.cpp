/* NPCHandler.cpp
 *
 * Copyright (C) 2004 Wow Daemon
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Affect.h"
#include "UpdateMask.h"

//////////////////////////////////////////////////////////////
/// This function handles MSG_TABARDVENDOR_ACTIVATE:
//////////////////////////////////////////////////////////////
void WorldSession::HandleTabardVendorActivateOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;
    recv_data >> guid;
    data.Initialize( MSG_TABARDVENDOR_ACTIVATE );
    data << guid;
    SendPacket( &data );
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_BANKER_ACTIVATE:
//////////////////////////////////////////////////////////////
void WorldSession::HandleBankerActivateOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;
    recv_data >> guid;

    data.Initialize( SMSG_SHOW_BANK );
    data << guid;
    SendPacket( &data );
}


/* CMSG_TRAINER_LIST: //needs to be changed to the vendor-list
void WorldSession::HandleTrainerListOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 player_level, player_gold;
    player_level = GetPlayer( )->GetUInt32Value( UNIT_FIELD_LEVEL );
    player_gold = GetPlayer( )->GetUInt32Value( PLAYER_FIELD_COINAGE );
    uint32 guid1, guid2;
    uint32 count;
    //count = 2; //we can have more then 2 spells now ;)
    recv_data >> guid1 >> guid2;

    DatabaseInterface *dbi = Database::getSingleton().createDatabaseInterface(); //
    count = (uint32)dbi->getTrainerSpellsCount ( session );
    data.Initialize( (38*count)+48, SMSG_TRAINER_LIST ); //set packet size - count = number of spells
    data << guid1 << guid2;
    data << uint32(0) << count;

    dbi->getTrainerSpells( session, data);
    Database::getSingleton().removeDatabaseInterface( dbi );

    SendPacket( &data );
}
*/


//////////////////////////////////////////////////////////////
/// This function handles CMSG_TRAINER_LIST
//////////////////////////////////////////////////////////////
void WorldSession::HandleTrainerListOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;
    uint32 cnt;

    recv_data >> guid;
    Trainerspell *strainer = objmgr.GetTrainerspell(GUID_LOPART(guid));

    cnt = 0;
    if(strainer)
    {
        // Log::getSingleton().outString("loading trainer %u with skillines %u, %u, and %u",GUID_LOPART(guid),strainer->skilline1,strainer->skilline2,strainer->skilline3);
        for (unsigned int t = 0;t < sSkillStore.GetNumRows();t++)
        {
            skilllinespell *skill = sSkillStore.LookupEntry(t);
            if ((skill->skilline == strainer->skilline1) || (skill->skilline == strainer->skilline2) || (skill->skilline == strainer->skilline3))
            {
                // Log::getSingleton().outString("skill %u with skillline %u matches",skill->spell,skill->skilline);
                SpellEntry *proto = sSpellStore.LookupEntry(skill->spell);
                if ((proto) && (proto->spellLevel != 0))
                {
                    cnt++;
                }
            }
        }
        data.Initialize( SMSG_TRAINER_LIST );     //set packet size - count = number of spells
        data << guid;
        data << uint32(0) << uint32(cnt);
        // Log::getSingleton().outString("count = %u",cnt);
        for (unsigned int t = 0;t < sSkillStore.GetNumRows();t++)
        {
            skilllinespell *skill = sSkillStore.LookupEntry(t);
            if ((skill->skilline == strainer->skilline1) || (skill->skilline == strainer->skilline2) || (skill->skilline == strainer->skilline3))
            {
                SpellEntry *proto = sSpellStore.LookupEntry(skill->spell);
                if ((proto) && (proto->spellLevel != 0))
                {
                    // Log::getSingleton( ).outString( "WORLD: Grabbing trainer spell %u with skilline %u", skill->spell, skill->skilline);
                    data << uint32(skill->spell);
                    // data << uint32(10);
                    if (GetPlayer()->HasSpell(skill->spell))
                    {
                        data << uint8(2);
                    }
                    else
                    {
                        if (((GetPlayer()->GetUInt32Value( UNIT_FIELD_LEVEL )) < (proto->spellLevel)) || (GetPlayer()->GetUInt32Value( PLAYER_FIELD_COINAGE ) < sWorld.mPrices[proto->spellLevel]))
                        {
                            data << uint8(1);
                        }
                        else
                        {
                            data << uint8(0);
                        }
                    }
                    data << uint32(sWorld.mPrices[proto->spellLevel]) << uint32(0);
                    data << uint32(0) << uint8(proto->spellLevel);
                    data << uint32(0);            // set type
                    data << uint32(0);            // set required level of a skill line
                    data << uint32(0);
                    data << uint32(0) << uint32(0);
                    // Log::getSingleton( ).outString( "WORLD: Grabbing trainer spell %u", itr->second->spell);
                }
            }
        }
        data << "Hello! Ready for some training?";
        SendPacket( &data );
    }
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_TRAINER_BUY_SPELL:
//////////////////////////////////////////////////////////////
void WorldSession::HandleTrainerBuySpellOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;
    uint32 spellId, playerGold, price;

    uint64 trainer = GetPlayer()->GetSelection();
    recv_data >> guid >> spellId;
    playerGold = GetPlayer( )->GetUInt32Value( PLAYER_FIELD_COINAGE );

    data.Initialize( SMSG_TRAINER_BUY_SUCCEEDED );
    data << guid << spellId;
    SendPacket( &data );
    SpellEntry *proto = sSpellStore.LookupEntry(spellId);
    price = sWorld.mPrices[proto->spellLevel];

    if( playerGold >= price )
    {
        GetPlayer( )->SetUInt32Value( PLAYER_FIELD_COINAGE, playerGold - price );

        // Ignatich: do we really need that spell casting sequence? need to check against logs

        data.Initialize( SMSG_SPELL_START );
        data << guid;
        data << guid;
        data << spellId;
        data << uint16(0);
        data << uint32(0);
        data << uint16(2);
        data << GetPlayer()->GetGUID();
        WPAssert(data.size() == 36);
        SendPacket( &data );

        data.Initialize( SMSG_LEARNED_SPELL );
        data << spellId;
        SendPacket( &data );
        GetPlayer()->addSpell((uint16)spellId);

        data.Initialize( SMSG_SPELL_GO );
        data << guid;
        data << guid;
        data << spellId;
        data << uint8(0) << uint8(1) << uint8(1);
        data << GetPlayer()->GetGUID();
        data << uint8(0);
        data << uint16(2);
        data << GetPlayer()->GetGUID();
        WPAssert(data.size() == 42);
        SendPacket( &data );

        data.Initialize( SMSG_SPELLLOGEXECUTE );
        data << guid;
        data << spellId;
        data << uint32(1);
        data << uint32(0x24);
        data << uint32(1);
        data << GetPlayer()->GetGUID();
        WPAssert(data.size() == 32);
        SendPacket( &data );
    }
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_PETITION_SHOWLIST:
//////////////////////////////////////////////////////////////
void WorldSession::HandlePetitionShowListOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;
    unsigned char tdata[21] =
    {
        0x01, 0x01, 0x00, 0x00, 0x00, 0xe7, 0x16, 0x00, 0x00, 0xef, 0x23, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
    };
    recv_data >> guid;
    data.Initialize( SMSG_PETITION_SHOWLIST );
    data << guid;
    data.append( tdata, sizeof(tdata) );
    SendPacket( &data );
}


//////////////////////////////////////////////////////////////
/// This function handles MSG_AUCTION_HELLO:
//////////////////////////////////////////////////////////////
void WorldSession::HandleAuctionHelloOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint64 guid;

    recv_data >> guid;

    data.Initialize( MSG_AUCTION_HELLO );
    data << guid;
    data << uint32(0);

    SendPacket( &data );
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_GOSSIP_HELLO:
//////////////////////////////////////////////////////////////
void WorldSession::HandleGossipHelloOpcode( WorldPacket & recv_data )
{
    Log::getSingleton( ).outString( "WORLD: Recieved CMSG_GOSSIP_HELLO" );
    WorldPacket data;
    uint64 guid;
    GossipNpc *pGossip;

    recv_data >> guid;

    pGossip = objmgr.GetGossipByGuid(GUID_LOPART(guid),GetPlayer()->GetMapId());

    if(pGossip)
    {
        data.Initialize( SMSG_GOSSIP_MESSAGE );
        data << guid;
        data << pGossip->TextID;
        data << pGossip->OptionCount;

        for(uint32 i=0; i<pGossip->OptionCount; i++)
        {
            data << i;
            data << pGossip->pOptions[i].Icon;
            data << pGossip->pOptions[i].OptionText;
        }

        // QUEST HANDLER
        data << uint32(0);                        //quest count
        SendPacket(&data);
    }
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_GOSSIP_SELECT_OPTION:
//////////////////////////////////////////////////////////////
void WorldSession::HandleGossipSelectOptionOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 option;
    uint64 guid;
    GossipNpc *pGossip;

    recv_data >> guid >> option;
    Log::getSingleton( ).outDetail("WORLD: CMSG_GOSSIP_SELECT_OPTION Option %i Guid %.8X\n", option, guid );

    pGossip = objmgr.GetGossipByGuid(GUID_LOPART(guid),GetPlayer()->GetMapId());

    switch(pGossip->pOptions[option].Special)
    {
        case GOSSIP_POI:
        {

        }break;
        case GOSSIP_SPIRIT_HEALER_ACTIVE:
        {
            data.Initialize( SMSG_SPIRIT_HEALER_CONFIRM );
            data << guid;
            SendPacket( &data );
            data.Initialize( SMSG_GOSSIP_COMPLETE );
            SendPacket( &data );
        }break;
        case GOSSIP_VENDOR:
        {

        }break;
        case GOSSIP_TRAINER:
        {

        }break;

        default: break;
    }

}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_SPIRIT_HEALER_ACTIVATE:
//////////////////////////////////////////////////////////////
void WorldSession::HandleSpiritHealerActivateOpcode( WorldPacket & recv_data )
{
    Affect *aff;

    SpellEntry *spellInfo = sSpellStore.LookupEntry( 2146 );
    if(spellInfo)
    {
        aff = new Affect(spellInfo,600000,GetPlayer()->GetGUID());
        GetPlayer( )->AddAffect(aff);
    }

    GetPlayer( )->DeathDurabilityLoss(0.25);
    GetPlayer( )->SetMovement(MOVE_LAND_WALK);
    GetPlayer( )->SetPlayerSpeed(RUN, (float)7.5, true);
    GetPlayer( )->SetPlayerSpeed(SWIM, (float)4.9, true);

    GetPlayer( )->SetUInt32Value(CONTAINER_FIELD_SLOT_1+29, 0);
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_AURA+32, 0);
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_AURALEVELS+8, 0xeeeeeeee);
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS+8, 0xeeeeeeee);
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_AURAFLAGS+4, 0);
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_AURASTATE, 0);

    GetPlayer( )->ResurrectPlayer();
    GetPlayer( )->SetUInt32Value(UNIT_FIELD_HEALTH, (uint32)(GetPlayer()->GetUInt32Value(UNIT_FIELD_MAXHEALTH)*0.50) );
    GetPlayer( )->SpawnCorpseBones();
}


//////////////////////////////////////////////////////////////
/// This function handles CMSG_NPC_TEXT_QUERY:
//////////////////////////////////////////////////////////////
void WorldSession::HandleNpcTextQueryOpcode( WorldPacket & recv_data )
{
    WorldPacket data;
    uint32 textID;
    uint32 uField0, uField1;
    GossipText *pGossip;

    recv_data >> textID;
    Log::getSingleton( ).outDetail("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID );

    recv_data >> uField0 >> uField1;
    GetPlayer()->SetUInt32Value(UNIT_FIELD_TARGET, uField0);
    GetPlayer()->SetUInt32Value(UNIT_FIELD_TARGET + 1, uField1);

    pGossip = objmgr.GetGossipText(textID);
    if(pGossip)
    {
        data.Initialize( SMSG_NPC_TEXT_UPDATE );
        data << textID;
        data << uint8(0x00) << uint8(0x00) << uint8(0x00) << uint8(0x00);
        data << pGossip->Text.c_str();
        SendPacket( &data );
    }
}


void WorldSession::HandleBinderActivateOpcode( WorldPacket & recv_data )
{
    GetPlayer( )->SetUInt32Value( UNIT_FIELD_FLAGS, (0xffffffff - 65536) & GetPlayer( )->GetUInt32Value( UNIT_FIELD_FLAGS ) );
    GetPlayer( )->SetUInt32Value( UNIT_FIELD_AURA +32, 0 );
    GetPlayer( )->SetUInt32Value( UNIT_FIELD_AURAFLAGS +4, 0 );
    GetPlayer( )->SetUInt32Value( UNIT_FIELD_AURASTATE, 0 );
    GetPlayer( )->SetUInt32Value( PLAYER_BYTES_2, (0xffffffff - 0x10) & GetPlayer( )->GetUInt32Value( PLAYER_BYTES_2 ) );
    //GetPlayer( )->UpdateObject( );
    GetPlayer()->setDeathState(ALIVE);
}
