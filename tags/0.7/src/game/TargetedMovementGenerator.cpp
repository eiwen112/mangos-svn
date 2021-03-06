/* 
 * Copyright (C) 2005,2006,2007 MaNGOS <http://www.mangosproject.org/>
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

#include "ByteBuffer.h"
#include "TargetedMovementGenerator.h"
#include "Errors.h"
#include "Creature.h"
#include "MapManager.h"
#include "Spell.h"
#include "DestinationHolderImp.h"

#define SMALL_ALPHA 0.05

#include <cmath>

struct StackCleaner
{
    Creature &i_creature;
    StackCleaner(Creature &creature) : i_creature(creature) {}
    void Done(void) { i_creature.StopMoving(); }
    ~StackCleaner()
    {
        i_creature->Clear();
    }
};

void
TargetedMovementGenerator::_setTargetLocation(Creature &owner)
{
    if( !&i_target || !&owner )
        return;

    if( owner.hasUnitState(UNIT_STAT_ROOT) || owner.hasUnitState(UNIT_STAT_STUNDED) )
        return;

    // prevent redundant micro-movement for pets, other followers.
    if(i_offset && i_target->IsWithinDistInMap(&owner,2*i_offset))
        return;

    float x, y, z;
    if(!i_offset)
    {
        // to nearest contact position
        i_target->GetContactPoint( &owner, x, y, z );
    }
    else
    {
        // to at i_offset distance from target and i_angle from target facing
        i_target->GetClosePoint(NULL,x,y,z,owner.GetObjectSize() + i_offset,i_angle);
    }

    //We don't update Mob Movement, if the difference between New destination and last destination is < BothObjectSize
    float  bothObjectSize = i_target->GetObjectSize() + owner.GetObjectSize() + CONTACT_DISTANCE;
    if( i_destinationHolder.HasDestination() && i_destinationHolder.GetDestinationDiff(x,y,z) < bothObjectSize )
        return;
    Traveller<Creature> traveller(owner);
    i_destinationHolder.SetDestination(traveller, x, y, z);
    owner.addUnitState(UNIT_STAT_CHASE);
}

void
TargetedMovementGenerator::Initialize(Creature &owner)
{
    if(!&owner)
        return;
    owner.setMoveRunFlag(true);
    _setTargetLocation(owner);
}

void
TargetedMovementGenerator::Reset(Creature &owner)
{
    Initialize(owner);
}

bool
TargetedMovementGenerator::Update(Creature &owner, const uint32 & time_diff)
{
    if(!i_target.isValid())
    {
        return false;
    }
    if( !&owner || !owner.isAlive() || !&i_target )
        return true;
    if( owner.hasUnitState(UNIT_STAT_ROOT) || owner.hasUnitState(UNIT_STAT_STUNDED) || owner.hasUnitState(UNIT_STAT_FLEEING))
        return true;
    if( !owner.isInCombat() && !owner.hasUnitState(UNIT_STAT_FOLLOW) )
    {
        //owner.AIM_Initialize();   This case must be the one, when a creature aggroed you. By Initalized a new AI, we prevented to Ai::_stopAttack() to be executed properly.
        return true;
    }

    // prevent crash after creature killed pet
    if (!owner.hasUnitState(UNIT_STAT_FOLLOW) && owner.getVictim() != i_target.getTarget())
        return true;

    Traveller<Creature> traveller(owner);

    if( !i_destinationHolder.HasDestination() )
        _setTargetLocation(owner);
    if( owner.IsStopped() && !i_destinationHolder.HasArrived() )
    {
        owner.addUnitState(UNIT_STAT_CHASE);
        i_destinationHolder.StartTravel(traveller);
        return true;
    }

    if (i_destinationHolder.UpdateTraveller(traveller, time_diff, false))
    {
        // put targeted movement generators on a higher priority
        if (owner.GetObjectSize())
            i_destinationHolder.ResetUpdate(50);

        float  dist = i_target->GetObjectSize() + owner.GetObjectSize() + CONTACT_DISTANCE;

        // try to counter precision differences
        if( i_destinationHolder.GetDistanceFromDestSq(*i_target.getTarget()) > dist * dist + 0.8)
        {
            owner.SetInFront(i_target.getTarget());         // Set new Angle For Map::
            _setTargetLocation(owner);                      //Calculate New Dest and Send data To Player
        }
        // Update the Angle of the target only for Map::, no need to send packet for player
        else if ( !i_angle && !owner.HasInArc( 0.01f, i_target.getTarget() ) )
            owner.SetInFront(i_target.getTarget());

        if( !owner.IsStopped() && i_destinationHolder.HasArrived())
        {
            //Angle update will take place into owner.StopMoving()
            owner.SetInFront(i_target.getTarget());

            owner.StopMoving();
            if(owner.canReachWithAttack(i_target.getTarget()) && !owner.hasUnitState(UNIT_STAT_FOLLOW))
                owner.Attack(i_target.getTarget());
        }
    }
    return true;
}

void TargetedMovementGenerator::_spellAtack(Creature &owner, SpellEntry* spellInfo)
{
    if(!spellInfo)
        return;
    owner.StopMoving();
    owner->Idle();
    if(owner.m_currentSpell)
    {
        if(owner.m_currentSpell->m_spellInfo->Id == spellInfo->Id )
            return;
        else
        {
            owner.m_currentSpell->cancel();
        }
    }
    Spell *spell = new Spell(&owner, spellInfo, false, 0);
    spell->SetAutoRepeat(true);
    //owner.addUnitState(UNIT_STAT_ATTACKING);
    owner.Attack(&owner);                                   //??
    owner.clearUnitState(UNIT_STAT_CHASE);
    SpellCastTargets targets;
    targets.setUnitTarget( i_target.getTarget() );
    spell->prepare(&targets);
    owner.m_canMove = false;
    DEBUG_LOG("Spell Attack.");
}

void TargetedMovementGenerator::spellAtack(Creature &owner,Unit &who,uint32 spellId)
{
    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId );

    if(!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %i\n", spellId);
        return;
    }

    owner.StopMoving();
    owner->Idle();
    if(owner.m_currentSpell)
    {
        if(owner.m_currentSpell->m_spellInfo->Id == spellInfo->Id )
            return;
        else
        {
            owner.m_currentSpell->cancel();
        }
    }
    Spell *spell = new Spell(&owner, spellInfo, false, 0);
    spell->SetAutoRepeat(false);
    //owner.addUnitState(UNIT_STAT_ATTACKING);
    //owner.clearUnitState(UNIT_STAT_CHASE);
    SpellCastTargets targets;
    targets.setUnitTarget( &who );
    spell->prepare(&targets);
    owner.m_canMove = false;
    DEBUG_LOG("Spell Attack.");
}
