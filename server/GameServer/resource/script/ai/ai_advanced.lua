print("-- [Loading] AI Advanced - Enhanced Monster AI System")
--------------------------------------------------------------
-- AI ADVANCED: Improved monster behavior system
-- Features:
--   1. Multi-waypoint patrol with varied timing
--   2. Priority targeting (healers, low HP targets)
--   3. Enhanced aggro with role-based threat
--   4. Thief/Goblin AI that steals ground loot
--   5. Group coordination and flanking
--   6. Alert states before full aggro
--------------------------------------------------------------

-- New AI type constants
AI_PATROL_ADV   = 50   -- Advanced patrol (multi-waypoint)
AI_SMART_ATK    = 51   -- Smart attacker (prioritizes healers)
AI_THIEF        = 52   -- Thief/Goblin (steals loot)
AI_GUARDIAN     = 53   -- Guardian (protects area, calls allies)
AI_AMBUSHER     = 54   -- Ambusher (waits hidden, surprise attack)

-- Healer job IDs (prioritized targets)
HEALER_JOBS = {}
HEALER_JOBS[5]  = true  -- Herbalist
HEALER_JOBS[13] = true  -- Cleric
HEALER_JOBS[14] = true  -- Seal Master

-- Threat multipliers by job
THREAT_MULT = {}
THREAT_MULT[5]  = 2.0   -- Herbalist generates extra threat
THREAT_MULT[13] = 2.5   -- Cleric generates extra threat
THREAT_MULT[14] = 1.8   -- Seal Master
THREAT_MULT[8]  = 0.8   -- Champion (tank, lower threat from heals)

-- Alert state tracking (per mob)
ai_alert_state = {}      -- 0=idle, 1=suspicious, 2=alert, 3=combat
ai_alert_timer = {}      -- time remaining in alert state
ai_patrol_waypoints = {} -- multi-waypoint storage
ai_patrol_index = {}     -- current waypoint index
ai_patrol_wait = {}      -- wait time at current waypoint
ai_thief_cooldown = {}   -- cooldown after stealing

--------------------------------------------------------------
-- ENHANCED PATROL SYSTEM
--------------------------------------------------------------

-- Set patrol waypoints for a mob: ai_set_patrol_route(c, {x1,y1, x2,y2, x3,y3...})
function ai_set_patrol_route(c, waypoints)
    local id = GetChaTypeID(c)
    ai_patrol_waypoints[c] = waypoints
    ai_patrol_index[c] = 1
    ai_patrol_wait[c] = 0
end

function ai_advanced_patrol(c)
    local waypoints = ai_patrol_waypoints[c]
    if waypoints == nil or #waypoints < 2 then
        -- Fallback to birth random move with varied timing
        birth_rand_move(c, 500 + Rand(300))
        return
    end

    -- Check if waiting at waypoint
    if ai_patrol_wait[c] ~= nil and ai_patrol_wait[c] > 0 then
        ai_patrol_wait[c] = ai_patrol_wait[c] - 1
        -- Random idle behavior while waiting
        if Rand(30) == 1 then
            rand_move(c, 100)  -- Small fidget movement
        end
        return
    end

    local idx = ai_patrol_index[c] or 1
    local wx = waypoints[idx]
    local wy = waypoints[idx + 1]

    if wx == nil or wy == nil then
        ai_patrol_index[c] = 1
        wx = waypoints[1]
        wy = waypoints[2]
    end

    -- Check if arrived at waypoint
    if is_near_pos(c, wx, wy, 60) == 1 then
        -- Wait at waypoint for a random time (natural behavior)
        ai_patrol_wait[c] = 3 + Rand(8)

        -- Move to next waypoint
        ai_patrol_index[c] = idx + 2
        if ai_patrol_index[c] > #waypoints then
            ai_patrol_index[c] = 1
        end
    else
        -- Move toward waypoint with slight randomness (natural path)
        local rx = wx + Rand(40) - 20
        local ry = wy + Rand(40) - 20
        if is_pos_valid(c, rx, ry) == 1 then
            ChaMove(c, rx, ry)
        else
            ChaMove(c, wx, wy)
        end
    end
end

--------------------------------------------------------------
-- PRIORITY TARGET SELECTION
--------------------------------------------------------------

-- Find the best target considering: healers > low HP > closest
function ai_find_priority_target(c)
    local x, y = GetChaPos(c)
    local vision = GetChaVision(c)

    -- Get multiple potential targets
    local targets = {}
    local t1, t2, t3, t4 = GetChaSetByRange(c, x, y, vision, 0)
    if t1 ~= nil then table.insert(targets, t1) end
    if t2 ~= nil then table.insert(targets, t2) end
    if t3 ~= nil then table.insert(targets, t3) end
    if t4 ~= nil then table.insert(targets, t4) end

    if #targets == 0 then
        return nil
    end

    local best_target = nil
    local best_priority = -1

    for _, t in ipairs(targets) do
        local priority = 0

        -- Check if target is in safe zone
        if IsChaInRegion(t, 2) == 1 then
            priority = -100
        else
            -- Priority 1: Is it a healer?
            local job = GetChaAttr(t, ATTR_JOB)
            if HEALER_JOBS[job] then
                priority = priority + 50
            end

            -- Priority 2: Low HP percentage (finish off weak targets)
            local hp = GetChaAttr(t, ATTR_HP)
            local mxhp = GetChaAttr(t, ATTR_MXHP)
            if mxhp > 0 then
                local hp_pct = hp / mxhp
                if hp_pct < 0.3 then
                    priority = priority + 30
                elseif hp_pct < 0.5 then
                    priority = priority + 15
                end
            end

            -- Priority 3: Proximity bonus
            local tx, ty = GetChaPos(t)
            local dist = math.sqrt((tx - x) * (tx - x) + (ty - y) * (ty - y))
            priority = priority + (vision - dist) / vision * 10
        end

        if priority > best_priority then
            best_priority = priority
            best_target = t
        end
    end

    return best_target
end

--------------------------------------------------------------
-- ENHANCED AGGRO SYSTEM
--------------------------------------------------------------

-- Better target update considering threat and priorities
function ai_smart_update_target(c, current_target)
    -- Check if current target is still valid
    if current_target == nil then
        return nil
    end

    local vision = GetChaVision(c)
    if is_near(c, current_target, vision) == 0 then
        return nil
    end

    -- Check if out of chase range
    if is_chase(c) == 0 then
        clear_target(c)
        local x, y = GetChaSpawnPos(c)
        ChaMoveToSleep(c, x, y)
        set_moving_back(c, 1)
        return nil
    end

    -- Periodically re-evaluate target (not every tick - natural feel)
    if Rand(100) < 20 then  -- 20% chance per tick to re-evaluate
        local new_target = ai_find_priority_target(c)
        if new_target ~= nil and new_target ~= current_target then
            -- Only switch if new target is significantly better
            local new_job = GetChaAttr(new_target, ATTR_JOB)
            local cur_job = GetChaAttr(current_target, ATTR_JOB)

            -- Always switch to a healer
            if HEALER_JOBS[new_job] and not HEALER_JOBS[cur_job] then
                return new_target
            end

            -- Switch to low HP target for finishing blow
            local new_hp = GetChaAttr(new_target, ATTR_HP)
            local new_mxhp = GetChaAttr(new_target, ATTR_MXHP)
            if new_mxhp > 0 and (new_hp / new_mxhp) < 0.2 then
                return new_target
            end
        end
    end

    return current_target
end

--------------------------------------------------------------
-- ALERT STATE SYSTEM
--------------------------------------------------------------

-- Transition: idle -> suspicious -> alert -> combat
function ai_update_alert(c)
    local state = ai_alert_state[c] or 0
    local timer = ai_alert_timer[c] or 0

    if state == 0 then
        -- Idle: check if player nearby (outer vision range)
        local vision = GetChaVision(c)
        local outer_range = vision + 200  -- Detection range larger than attack range
        local t = find_target(c, 0)
        if t ~= nil then
            -- Player detected! Become suspicious
            ai_alert_state[c] = 1
            ai_alert_timer[c] = 3 + Rand(3)  -- 3-5 ticks suspicious
            SetChaEmotion(c, 5)  -- Show "?" emotion
            -- Face toward the player
            local tx, ty = GetChaPos(t)
            -- Move slightly toward player (curious)
            local mx, my = GetChaPos(c)
            local dx = (tx - mx) * 0.3
            local dy = (ty - my) * 0.3
            local nx = mx + dx
            local ny = my + dy
            if is_pos_valid(c, nx, ny) == 1 then
                ChaMove(c, nx, ny)
            end
            return 1  -- Handled
        end
    elseif state == 1 then
        -- Suspicious: look around, face player
        timer = timer - 1
        if timer <= 0 then
            -- Escalate to alert
            ai_alert_state[c] = 2
            ai_alert_timer[c] = 2
            SetChaEmotion(c, 11)  -- Show "!" emotion
        else
            ai_alert_timer[c] = timer
            -- Random look-around movement
            if Rand(3) == 1 then
                rand_move(c, 80)
            end
        end
        return 1  -- Handled
    elseif state == 2 then
        -- Alert: about to attack
        timer = timer - 1
        if timer <= 0 then
            -- Go to combat!
            ai_alert_state[c] = 3
            local t = find_target(c, 0)
            if t ~= nil then
                SetChaTarget(c, t)
            end
        else
            ai_alert_timer[c] = timer
        end
        return 1
    end

    return 0  -- Not handled, proceed with normal AI
end

-- Reset alert state when target is lost
function ai_reset_alert(c)
    ai_alert_state[c] = 0
    ai_alert_timer[c] = 0
end

--------------------------------------------------------------
-- THIEF / GOBLIN AI (AI_THIEF = 52)
--------------------------------------------------------------

function ai_thief_behavior(c, t)
    local cooldown = ai_thief_cooldown[c] or 0

    if cooldown > 0 then
        -- On cooldown after stealing: flee!
        ai_thief_cooldown[c] = cooldown - 1
        if t ~= nil then
            flee(c, t)
        else
            -- Run back to spawn
            local sx, sy = GetChaSpawnPos(c)
            ChaMove(c, sx, sy)
        end
        return
    end

    -- Priority 1: Look for ground loot to steal
    local item = FindItem(c, 600)
    if item ~= nil then
        -- Found loot! Move toward it and pick it up
        local ix, iy = GetItemPos(item)
        if is_near_pos(c, ix, iy, 80) == 1 then
            -- Close enough to steal!
            PickItem(c, item)
            ai_thief_cooldown[c] = 10 + Rand(5)  -- Flee after stealing
            SetChaEmotion(c, 10)  -- Excited emotion
        else
            -- Move toward the item
            ChaMove(c, ix, iy)
        end
        return
    end

    -- Priority 2: If has target, do hit-and-run attacks
    if t ~= nil then
        if is_near(c, t, 200) == 1 then
            -- Too close! Attack then flee
            local skill_id = select_skill(c)
            ChaUseSkill(c, t, skill_id)
            ai_thief_cooldown[c] = 3 + Rand(3)  -- Short flee after attack
        else
            -- Circle around the target (don't approach directly)
            local tx, ty = GetChaPos(t)
            local cx, cy = GetChaPos(c)
            -- Move perpendicular to target
            local dx = tx - cx
            local dy = ty - cy
            local nx = cx - dy * 0.5 + Rand(100) - 50
            local ny = cy + dx * 0.5 + Rand(100) - 50
            if is_pos_valid(c, nx, ny) == 1 then
                ChaMove(c, nx, ny)
            end
        end
    else
        -- No target, no loot: wander looking for items
        rand_move(c, 500)
    end
end

-- Thief idle behavior: actively seek loot
function ai_thief_idle(c)
    -- First check for ground items
    local item = FindItem(c, 800)
    if item ~= nil then
        local ix, iy = GetItemPos(item)
        if is_near_pos(c, ix, iy, 80) == 1 then
            PickItem(c, item)
            ai_thief_cooldown[c] = 8 + Rand(5)
            SetChaEmotion(c, 10)
        else
            ChaMove(c, ix, iy)
        end
        return
    end

    -- Otherwise sneak around
    local x, y = GetChaSpawnPos(c)
    if is_near_pos(c, x, y, 100) == 1 then
        -- Near spawn: wander farther to look for loot
        rand_move(c, 600)
    else
        -- Far from spawn: occasionally return
        if Rand(5) == 1 then
            ChaMove(c, x, y)
        else
            rand_move(c, 400)
        end
    end
end

--------------------------------------------------------------
-- GROUP COORDINATION
--------------------------------------------------------------

-- When one mob is attacked, nearby mobs of same type become alert
function ai_group_alert(c, attacker)
    local x, y = GetChaPos(c)
    local m_type = GetChaTypeID(c)
    local m = {}
    m[0], m[1], m[2], m[3] = GetChaSetByRange(c, x, y, 600, m_type)
    for id, monster in pairs(m) do
        if monster ~= nil and monster ~= c then
            local state = ai_alert_state[monster] or 0
            if state < 2 then
                ai_alert_state[monster] = 2
                ai_alert_timer[monster] = 1 + Rand(2)
                SetChaEmotion(monster, 11)
            end
        end
    end
end

--------------------------------------------------------------
-- SMART SKILL SELECTION
--------------------------------------------------------------

-- Select skill based on situation (not purely random)
function ai_smart_select_skill(c, t)
    local num = GetChaSkillNum(c) - 1
    if num <= 0 then
        return select_skill(c)
    end

    -- Check HP to decide if defensive skill is needed
    local hp = GetChaAttr(c, ATTR_HP)
    local mxhp = GetChaAttr(c, ATTR_MXHP)

    -- Low HP: prefer defensive/healing skills (higher numbered skills often)
    if mxhp > 0 and (hp / mxhp) < 0.3 then
        -- Try to use last skill (often self-buff/heal in this game)
        local skill_id, ratio = GetChaSkillInfo(c, num)
        if skill_id ~= nil and skill_id > 0 then
            if Rand(100) < 60 then  -- 60% chance to use defensive skill
                return skill_id
            end
        end
    end

    -- Normal: use weighted random (existing system)
    return select_skill(c)
end

--------------------------------------------------------------
-- MAIN AI LOOP OVERRIDE
--------------------------------------------------------------

-- Enhanced ai_loop that integrates all improvements
function ai_loop_advanced(c)
    local ai_t = GetChaAIType(c)

    -- Skip if no AI
    if ai_t == AI_NONE then return end

    -- Handle thief AI separately
    if ai_t == AI_THIEF then
        local t = GetChaTarget(c)
        if t ~= nil then
            if ai_target_safe(c, t) == 0 then
                ai_thief_behavior(c, t)
            else
                clear_target(c)
                ai_thief_idle(c)
            end
        else
            -- Try to find a target or look for loot
            if ai_seek_target(c) == 0 then
                ai_thief_idle(c)
            end
        end
        ai_block(c, GetChaTarget(c))
        return
    end

    -- Handle smart attacker
    if ai_t == AI_SMART_ATK then
        local t = GetChaTarget(c)
        if t ~= nil then
            if ai_target_safe(c, t) == 0 then
                -- Smart target update
                local new_t = ai_smart_update_target(c, t)
                if new_t == nil then
                    clear_target(c)
                elseif new_t ~= t then
                    clear_target(c)
                    SetChaTarget(c, new_t)
                    t = new_t
                end

                if t ~= nil then
                    local skill_id = ai_smart_select_skill(c, t)
                    ChaUseSkill(c, t, skill_id)
                end
            else
                clear_target(c)
            end
        else
            -- Use priority targeting to find best target
            local new_t = ai_find_priority_target(c)
            if new_t ~= nil then
                SetChaTarget(c, new_t)
                ai_group_alert(c, new_t)  -- Alert nearby allies
            else
                ai_advanced_patrol(c)
            end
        end
        ai_block(c, GetChaTarget(c))
        return
    end

    -- Handle advanced patrol type
    if ai_t == AI_PATROL_ADV then
        local t = GetChaTarget(c)
        if t ~= nil then
            if ai_target_safe(c, t) == 0 then
                local skill_id = select_skill(c)
                ChaUseSkill(c, t, skill_id)
            else
                clear_target(c)
            end
        else
            if ai_seek_target(c) == 0 then
                ai_advanced_patrol(c)
            end
        end
        ai_block(c, GetChaTarget(c))
        return
    end

    -- Handle guardian type (protects area, calls allies aggressively)
    if ai_t == AI_GUARDIAN then
        local t = GetChaTarget(c)
        if t ~= nil then
            if ai_target_safe(c, t) == 0 then
                -- Always call for help
                if Rand(10) == 1 then
                    ai_group_alert(c, t)
                end
                local skill_id = ai_smart_select_skill(c, t)
                ChaUseSkill(c, t, skill_id)
            else
                clear_target(c)
            end
        else
            local new_t = ai_find_priority_target(c)
            if new_t ~= nil then
                SetChaTarget(c, new_t)
                ai_group_alert(c, new_t)
                SetChaEmotion(c, 11)
            else
                -- Guard position with minimal movement
                local sx, sy = GetChaSpawnPos(c)
                if is_near_pos(c, sx, sy, 150) == 0 then
                    ChaMove(c, sx, sy)
                elseif Rand(40) == 1 then
                    rand_move(c, 100)
                end
            end
        end
        ai_block(c, GetChaTarget(c))
        return
    end

    -- Handle ambusher type
    if ai_t == AI_AMBUSHER then
        local t = GetChaTarget(c)
        if t ~= nil then
            if ai_target_safe(c, t) == 0 then
                -- Hit hard then retreat
                local skill_id = select_skill(c)
                ChaUseSkill(c, t, skill_id)
                -- After attacking, sometimes flee to reset
                if Rand(5) == 1 then
                    flee(c, t)
                end
            else
                clear_target(c)
            end
        else
            -- Use alert system for ambush
            if ai_update_alert(c) == 0 then
                -- Stay near spawn (lurking)
                local sx, sy = GetChaSpawnPos(c)
                if is_near_pos(c, sx, sy, 80) == 0 then
                    ChaMove(c, sx, sy)
                end
            end
        end
        ai_block(c, GetChaTarget(c))
        return
    end

    -- Default: use existing ai_loop for standard types
    ai_loop(c)
end

--------------------------------------------------------------
-- OVERRIDE ai_timer to use enhanced system
--------------------------------------------------------------

-- Store original ai_timer reference
local original_ai_timer = ai_timer

function ai_timer(c, freq, time)
    local ResumeFreq = 10
    local Tick = GetChaParam(c, 1)
    SetChaParam(c, 1, Tick + freq * time)
    if math.fmod(Tick, ResumeFreq) == 0 and Tick > 0 then
        if IsChaLiving(c) == 1 then
            if GetChaAttr(c, ATTR_HP) < GetChaAttr(c, ATTR_MXHP) then
                Resume(c)
            end
        end
    end

    -- Use advanced AI loop for new AI types
    local ai_t = GetChaAIType(c)
    if ai_t >= 50 and ai_t <= 54 then
        ai_loop_advanced(c)
    else
        -- Use enhanced features for existing types too
        ai_loop_enhanced(c)
    end
end

-- Enhanced version of the standard ai_loop (backwards compatible)
function ai_loop_enhanced(c)
    local ai_t = GetChaAIType(c)
    if ai_t == AI_NONE then return end

    local t = GetChaTarget(c)
    if t ~= nil then
        if ai_target_safe(c, t) == 0 then
            ai_target_enhanced(c, t)
        end
    else
        ai_no_target_enhanced(c)
    end

    ai_tick(c, t)
end

-- Enhanced targeting (uses priority for existing aggressive types)
function ai_target_enhanced(c, t)
    local ai_type = GetChaAIType(c)

    if ai_type ~= 4 then
        if ai_update_target(c, t, ai_type) == 1 then
            return
        end
    end

    -- For aggressive types, periodically check for better targets
    if ai_type >= AI_R_ATK and ai_type ~= AI_FLEE then
        if Rand(100) < 15 then  -- 15% chance to re-evaluate
            local new_t = ai_find_priority_target(c)
            if new_t ~= nil and new_t ~= t then
                local new_job = GetChaAttr(new_t, ATTR_JOB)
                if HEALER_JOBS[new_job] then
                    clear_target(c)
                    SetChaTarget(c, new_t)
                    t = new_t
                end
            end
        end
    end

    if ai_type == AI_FLEE then
        flee(c, t)
        return
    elseif ai_type == 4 then
        star_move_to(c, t)
        return
    elseif ai_type == AI_ATK_FLEE then
        if is_near(c, t, 400) == 1 then
            flee(c, t)
            return
        end
    end

    if ai_type > 20 and ai_type < 50 then
        special_ai(c, t, ai_type)
    else
        local skill_id = select_skill(c)
        ChaUseSkill(c, t, skill_id)
    end

    if is_cha_can_summon(c) == 1 then
        summon_monster(c, t)
    end
end

-- Enhanced no-target behavior with more natural movement
function ai_no_target_enhanced(c)
    local ai_type = GetChaAIType(c)

    if ai_type == 4 then
        star_delete_to(c)
        return
    end

    local x, y = GetChaSpawnPos(c)

    if is_near_pos(c, x, y, 100) == 1 then
        SetChaPatrolState(c, 0)
    end

    if ai_seek_target(c) == 0 then
        if is_moving_back(c) == 0 then
            local now_x, now_y = GetChaPos(c)
            local dis = (now_x - x) * (now_x - x) + (now_y - y) * (now_y - y)

            if dis > 5000 then
                ChaMove(c, x, y)
            else
                -- More natural idle: vary between patrol and standing
                if is_patrol(c) == 0 then
                    -- Random movement with varied range and timing
                    local chance = Rand(30)
                    if chance == 1 then
                        birth_rand_move(c, 400 + Rand(300))
                    elseif chance == 2 then
                        -- Occasionally just change facing (small move)
                        rand_move(c, 50)
                    end
                else
                    ai_idle(c)
                end
            end
        else
            local move_flag = Rand(3)
            if move_flag == 1 then
                ChaMove(c, x, y)
            end
        end
    end
end

print("-- [Loaded] AI Advanced System - Types 50-54 active")
