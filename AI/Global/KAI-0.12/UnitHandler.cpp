#include "UnitHandler.h"
#include "MetalMaker.h"


CUnitHandler::CUnitHandler(AIClasses* ai) {
	this -> ai = ai;
	IdleUnits.resize(LASTCATEGORY);
	BuildTasks.resize(LASTCATEGORY);
	TaskPlans.resize(LASTCATEGORY);
	AllUnitsByCat.resize(LASTCATEGORY);
	AllUnitsByType.resize(ai -> cb -> GetNumUnitDefs() + 1);

	for (int i = 0; i <= ai -> cb -> GetNumUnitDefs(); i++) {
		AllUnitsByType[i] = new list<int>;
	}
	for (int i = 0; i < LASTCATEGORY; i++) {
		IdleUnits[i] = new list<int>;
		BuildTasks[i] = new list<BuildTask>;
		TaskPlans[i] = new list<TaskPlan>;
		AllUnitsByCat[i] = new list<int>;
	}

	taskPlanCounter = 1;
	metalMaker = new CMetalMaker(ai -> cb);
}

CUnitHandler::~CUnitHandler() {
	for (int i = 0; i < LASTCATEGORY; i++){
		delete IdleUnits[i];
		delete BuildTasks[i];
		delete TaskPlans[i];
		delete AllUnitsByCat[i];
	}

	for (int i = 0; i <= ai -> ut -> numOfUnits; i++){
		delete AllUnitsByType[i];
	}

	for (list<BuilderTracker*>::iterator i = BuilderTrackers.begin(); i != BuilderTrackers.end(); i++){
		delete *i;
	}
}



void CUnitHandler::IdleUnitUpdate() {
//	std::cout << "CUnitHandler::IdleUnitUpdate()" << std::endl;

	list<integer2> limboremoveunits;
	for (list<integer2>::iterator i = Limbo.begin(); i != Limbo.end(); i++) {
		if (i -> y > 0) {
			i -> y = i -> y - 1;
		}
		else {
			// L("adding unit to idle units: " << i -> x);
			if (ai -> cb -> GetUnitDef(i -> x) == NULL) {
				// L(" Removeing dead unit... ");
				
			}
			else
				IdleUnits[ai -> ut -> GetCategory(i -> x)] -> push_back(i -> x);
			if (ai -> ut -> GetCategory(i -> x) == CAT_BUILDER) {
				// it' ok now, stop the force idle (hack)
				// GetBuilderTracker(i -> x) -> idleStartFrame = -1;
			}

			limboremoveunits.push_back(*i);
		}
	}
	if (limboremoveunits.size()) {
		for (list<integer2>::iterator i = limboremoveunits.begin(); i != limboremoveunits.end(); i++) {
			Limbo.remove(*i);
		}
	}
	// make sure that all the builders are in action (hack?)
	if (ai -> cb -> GetCurrentFrame() % 15 == 0)
		for (list<BuilderTracker*>::iterator i = BuilderTrackers.begin(); i != BuilderTrackers.end(); i++) {
			// the new test
			if ((*i) -> idleStartFrame != -2) {
				// the brand new builders must be filtered still
				// L("VerifyOrder");
				bool ans = VerifyOrder(*i);
				const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands((*i) -> builderID);
				Command c;

				if (mycommands -> size() > 0)
					c = mycommands -> front();

				// two sec delay is ok
				if (( (*i) -> commandOrderPushFrame + LAG_ACCEPTANCE) < ai -> cb -> GetCurrentFrame()) {
					// assert(ans);
					if (!ans) {
						char text[512];
						float3 pos = ai -> cb -> GetUnitPos((*i) -> builderID);
						sprintf(text, "builder %i VerifyOrder failed ", (*i) -> builderID);
						AIHCAddMapPoint amp;
						amp.label = text;
						amp.pos = pos;
						////ai -> cb -> HandleCommand(&amp);

						ClearOrder(*i, false);
						if (!mycommands -> empty())
							DecodeOrder(*i, true);
						else // it's idle
							IdleUnitAdd((*i) -> builderID);
					}
				}
			}
		}
}

void CUnitHandler::UnitMoveFailed(int unit) {
	unit = unit;
}

void CUnitHandler::UnitCreated(int unit) {
	int category = ai -> ut -> GetCategory(unit);
	const UnitDef* newUnitDef = ai -> cb -> GetUnitDef(unit);

	if (category != -1) {
		// L("Unit " << unit << " created, ID : " << newUnitDef -> id << " Cat: " << category);
		AllUnitsByCat[category] -> push_back(unit);
		AllUnitsByType[newUnitDef -> id] -> push_back(unit);
		// L("push sucessful");
		if (category == CAT_FACTORY) {
			FactoryAdd(unit);
		}
		BuildTaskCreate(unit);	
		if (category == CAT_BUILDER) {
			// add the new builder
			BuilderTracker* builderTracker = new BuilderTracker;
			builderTracker -> builderID = unit;
			builderTracker -> buildTaskId = 0;
			builderTracker -> taskPlanId = 0;
			builderTracker -> factoryId = 0;
			builderTracker -> stuckCount = 0;
			builderTracker -> customOrderId = 0;
			builderTracker -> commandOrderPushFrame = -2; // Under construction
			builderTracker -> categoryMaker = -1;
			builderTracker -> idleStartFrame = -2; // Wait for the first idle call, as this unit might be under construction
			BuilderTrackers.push_back(builderTracker);
		}

		if (category == CAT_MMAKER) {
			MMakerAdd(unit);
		}
	}
}

void CUnitHandler::UnitDestroyed(int unit) {
	int category = ai -> ut -> GetCategory(unit);
	const UnitDef* unitDef = ai -> cb -> GetUnitDef(unit);

	if (category != -1) {
		AllUnitsByType[unitDef -> id] -> remove(unit);
		AllUnitsByCat[category] -> remove(unit);
		IdleUnitRemove(unit);
		BuildTaskRemove(unit);

		if (category == CAT_DEFENCE) {
			ai -> dm -> RemoveDefense(ai -> cb -> GetUnitPos(unit),unitDef);
		}
		if (category == CAT_MMAKER) {
			MMakerRemove(unit);
		}
		if (category == CAT_FACTORY) {
			FactoryRemove(unit);
		}

		if (category == CAT_BUILDER) {
			// remove the builder
			for (list<BuilderTracker*>::iterator i = BuilderTrackers.begin(); i != BuilderTrackers.end(); i++) {
				if ((*i) -> builderID == unit) {
					if ((*i) -> buildTaskId)
						BuildTaskRemove(*i);
					if ((*i) -> taskPlanId)
						TaskPlanRemove(*i);
					if ((*i) -> factoryId)
						FactoryBuilderRemove(*i);

					BuilderTracker* builderTracker = *i;
					BuilderTrackers.erase(i);
					delete builderTracker; // Test this
					break;
				}
			}
		}
	}
}

void CUnitHandler::IdleUnitAdd(int unit) {
	// L("IdleUnitAdd: " << unit);
	int category = ai -> ut -> GetCategory(unit);
	if (category != -1) {
		const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands(unit);

		if (mycommands -> empty()) {
			if (category == CAT_BUILDER) {
				BuilderTracker* builderTracker = GetBuilderTracker(unit);
				// L("it was a builder");
				// add clear here
				ClearOrder(builderTracker, true);

				if (builderTracker -> idleStartFrame == -2) {
					// it was in the idle list already?
					IdleUnitRemove(builderTracker -> builderID);
				}

				builderTracker -> idleStartFrame = -2; // it's in the idle list now

				if (builderTracker -> commandOrderPushFrame == -2) {
					// make sure that if the unit was just built it will have some time to leave the factory
					builderTracker -> commandOrderPushFrame = ai -> cb -> GetCurrentFrame() + 30 * 3;
				}
			}

			integer2 myunit(unit, LIMBOTIME);
			// L("Adding unit : " << myunit.x << " To Limbo " << myunit.y);
			Limbo.remove(myunit);
			// IdleUnitRemove(unit);  // This might be a better idea, but it's over the edge (possible assertion)
			Limbo.push_back(myunit);
		}
		else {
			// the unit has orders still
			if (category == CAT_BUILDER) {
				BuilderTracker* builderTracker = GetBuilderTracker(unit);
				assert(false);
				DecodeOrder(builderTracker, true);
			}
		}
	}
}

bool CUnitHandler::VerifyOrder(BuilderTracker* builderTracker) {
	// if it's without orders then try to find the lost command (TODO)
	// now take a look, and see what it's doing
	const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands(builderTracker -> builderID);
	bool commandFound = false;
	if (mycommands -> size() > 0) {
		// it has orders
		const Command* c = &mycommands -> front();

		if (mycommands -> size() == 2) {
			// it might have a reclaim order, or terrain change order
			// take command nr. 2
			c = &mycommands -> back();
		}

		// L("idle builder: " << builderTracker -> builderID);
		// L("c.id: " << c.id);
		// L("c.params[0]: " <<  c.params[0]);
		bool hit = false;
		if (builderTracker -> buildTaskId != 0) {
			hit = true;
			// test that this builder is on repair on this unit
			BuildTask* buildTask = GetBuildTask(builderTracker -> buildTaskId);
			if (c -> id == CMD_REPAIR && c -> params[0] == builderTracker -> buildTaskId
				|| (c -> id == -buildTask -> def -> id && c -> params[0] == buildTask -> pos.x && c -> params[2] == buildTask -> pos.z))
				commandFound = true;
			else
				return false;

		}
		if (builderTracker -> taskPlanId != 0) {
			assert(!hit);
			hit = true;
			TaskPlan* taskPlan = GetTaskPlan(builderTracker -> taskPlanId);
			
			if (c -> id == -taskPlan -> def -> id && c -> params[0] == taskPlan -> pos.x && c -> params[2] == taskPlan -> pos.z)
				commandFound = true;
			else
				return false;
		}
		if (builderTracker -> factoryId != 0) {
			assert(!hit);
			hit = true;
			if (c -> id == CMD_GUARD && c -> params[0] == builderTracker -> factoryId)
				commandFound = true;
			else
				return false;
		}
		if (builderTracker -> customOrderId != 0) {
			assert(!hit);
			hit = true;
			// CMD_MOVE is for human control stuff, CMD_REPAIR is for repairs of just made stuff
			// CMD_GUARD... ?
			if (c -> id == CMD_RECLAIM || c -> id == CMD_MOVE || c -> id == CMD_REPAIR)
				commandFound = true;
			else {
				// assert(false);
				return false;
			}
		}
		if (hit && commandFound) {
			// it's on the right job
			return true;
		}
	}
	else  {
		if (builderTracker -> idleStartFrame == -2) {
			return true;
		}
	}

	return false;
}

/*
Use this only if the unit dont have any orders at the moment
*/
void CUnitHandler::ClearOrder(BuilderTracker* builderTracker, bool reportError)
{
	bool hit = false;
	const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands(builderTracker -> builderID);
	assert(mycommands -> empty() || !reportError);
	if(builderTracker -> buildTaskId != 0)
	{
		// Hmm, why is this builder idle ???
		hit = true;
		//L("builder " << builderTracker -> builderID << " was idle, but it is on buildTaskId : " << builderTracker -> buildTaskId);
		BuildTask* buildTask = GetBuildTask(builderTracker -> buildTaskId);
		char text[512];
		sprintf(text, "builder %i: was idle, but it is on buildTaskId: %i  (stuck?)", builderTracker -> builderID, builderTracker -> buildTaskId);
		AIHCAddMapPoint amp;
		amp.label = text;
		amp.pos = buildTask -> pos;
		////ai -> cb -> HandleCommand(&amp);
		if(buildTask -> builderTrackers.size() > 1)
		{
			BuildTaskRemove(builderTracker);
		}
		else
		{
			// This is the only builder of this thing, and now its idle...
			BuildTaskRemove(builderTracker); // IS this smart at all ???
		}
		//return;
	}
	if(builderTracker -> taskPlanId != 0)
	{
		assert(!hit);
		hit = true;
		// Hmm, why is this builder idle ???
		// 
		TaskPlan* taskPlan = GetTaskPlan(builderTracker -> taskPlanId);
		//L("builder " << builderTracker -> builderID << " was idle, but it is on taskPlanId : " << taskPlan -> def -> humanName << " (masking this spot)");
		char text[512];
		sprintf(text, "builder %i: was idle, but it is on taskPlanId: %s (masking this spot)", builderTracker -> builderID, taskPlan -> def -> humanName.c_str());
		AIHCAddMapPoint amp;
		amp.label = text;
		amp.pos = taskPlan -> pos;
		////ai -> cb -> HandleCommand(&amp);
		ai -> dm -> MaskBadBuildSpot(taskPlan -> pos);
		// TODO: fix this:  Remove all builders from this plan.
		if(reportError)
		{
			list<BuilderTracker*> builderTrackers = taskPlan -> builderTrackers; // This is a copy of the list
			for(list<BuilderTracker*>::iterator i = builderTrackers.begin(); i != builderTrackers.end(); i++) {
				TaskPlanRemove(*i);
				ai -> MyUnits[(*i) -> builderID] -> Stop(); // Stop the units on the way to this plan
			}
		} else
		{
			TaskPlanRemove(builderTracker);
		}
		//return;
	}
	if(builderTracker -> factoryId != 0)
	{
		assert(!hit);
		hit = true;
		// Hmm, why is this builder idle ???
		//L("builder " << builderTracker -> builderID << " was idle, but it is on factoryId : " << builderTracker -> factoryId) << " (removing the builder from the job)";
		char text[512];
		sprintf(text, "builder %i: was idle, but it is on factoryId: %i (removing the builder from the job)", builderTracker -> builderID, builderTracker -> factoryId);
		AIHCAddMapPoint amp;
		amp.label = text;
		amp.pos = ai -> cb -> GetUnitPos(builderTracker -> factoryId);
		////ai -> cb -> HandleCommand(&amp);
		FactoryBuilderRemove(builderTracker);
	}
	if(builderTracker -> customOrderId != 0)
	{
		assert(!hit);
		hit = true;
		// Hmm, why is this builder idle ?
		// No tracking of custom orders yet... 
		//L("builder " << builderTracker -> builderID << " was idle, but it is on customOrderId : " << builderTracker -> customOrderId << " (removing the builder from the job)");
		
		//char text[512];
		//sprintf(text, "builder %i: was idle, but it is on customOrderId: %i (removing the builder from the job)", unit, builderTracker -> customOrderId);
		//AIHCAddMapPoint amp;
		//amp.label = text;
		//amp.pos = ai -> cb -> GetUnitPos(builderTracker -> builderID);
		//////ai -> cb -> HandleCommand(&amp);
		builderTracker -> customOrderId = 0;
	}
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
}

void CUnitHandler::DecodeOrder(BuilderTracker* builderTracker, bool reportError) {
	reportError = reportError;
	// If its without orders then try to find the lost command
	
	// TODO: All of it!!!!!!!!!!!!!!
	// Now take a look, and see what its doing:
	const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands(builderTracker -> builderID);
	if(mycommands -> size() > 0)
	{
		// It have orders
		const Command* c = &mycommands -> front();
		if(mycommands -> size() == 2 && c -> id == CMD_MOVE)//&& (c -> id == CMD_MOVE || c -> id == CMD_RECLAIM))  
		{
			// Hmm, it might have a move order before the real order
			// Take command nr. 2 if nr.1 is a move order
			c = &mycommands -> back();
		}
		
		
		//L("idle builder: " << builderTracker -> builderID);
		//L("c -> id: " << c -> id);
		//L("c -> params[0]: " <<  c -> params[0]);
		char text[512];
		sprintf(text, "builder %i: was clamed idle, but it have a command c -> id: %i, c -> params[0]: %f", builderTracker -> builderID, c -> id, c -> params[0]);
		AIHCAddMapPoint amp;
		amp.label = text;
		amp.pos = ai -> cb -> GetUnitPos(builderTracker -> builderID);
		////ai -> cb -> HandleCommand(&amp);
		if(c -> id < 0) // Its building a unit
		{
			float3 newUnitPos;
			newUnitPos.x = c -> params[0];
			newUnitPos.y = c -> params[1];
			newUnitPos.z = c -> params[2];
			// c.id == -newUnitDef -> id
			const UnitDef* newUnitDef = ai -> ut -> unittypearray[-c -> id].def;
			// Now make shure that no BuildTasks exists there 
			BuildTask* buildTask = BuildTaskExist(newUnitPos, newUnitDef);
			if(buildTask)
			{
				BuildTaskAddBuilder(buildTask, builderTracker);
			}
			else // Make a new TaskPlan (or join an existing one)
				TaskPlanCreate(builderTracker -> builderID, newUnitPos, newUnitDef);

		}
		if(c -> id == CMD_REPAIR)  // Its repairing    ( || c.id == CMD_GUARD)
		{
			int guardingID = int(c -> params[0]);
			// Find the unit its repairng
			int category = ai -> ut -> GetCategory(guardingID);
			if(category == -1)
				return; // This is bad....
			bool found = false;
			for(list<BuildTask>::iterator i = BuildTasks[category] -> begin(); i != BuildTasks[category] -> end(); i++){
				if(i -> id == guardingID)
				{
					// Whatever the old order was, update it now...
					bool hit = false;
					if(builderTracker -> buildTaskId != 0)
					{
						hit = true;
						// Hmm, why is this builder idle ???
						BuildTask* buildTask = GetBuildTask(builderTracker -> buildTaskId);
						if(buildTask -> builderTrackers.size() > 1)
						{
							BuildTaskRemove(builderTracker);
						}
						else
						{
							// This is the only builder of this thing, and now its idle...
							BuildTaskRemove(builderTracker); // IS this smart at all ???
						}
					}
					if(builderTracker -> taskPlanId != 0)
					{
						assert(!hit);
						hit = true;
						TaskPlanRemove(builderTracker);
						
						//return;
					}
					if(builderTracker -> factoryId != 0)
					{
						assert(!hit);
						hit = true;
						FactoryBuilderRemove(builderTracker);
					}
					if(builderTracker -> customOrderId != 0)
					{
						assert(!hit);
						hit = true;
						builderTracker -> customOrderId = 0;
					}
					BuildTask* bt = &*i;
					//L("Adding builder to BuildTask " << bt -> id << ": " << ai -> cb -> GetUnitDef(bt -> id) -> humanName);
					BuildTaskAddBuilder(bt, builderTracker);
					found = true;
				}
			}
			if(found == false)
			{
				// Not found, just make a custom order
				//L("Not found, just make a custom order");
				builderTracker -> customOrderId = taskPlanCounter++;
				builderTracker -> idleStartFrame = -1; // Its in use now
			}
		}
	}
	else
	{
		// Error: this function needs a builder with orders
		//L("Error: this function needs a builder with orders");
		assert(false);
	}
}

void CUnitHandler::IdleUnitRemove(int unit)
{
	int category = ai -> ut -> GetCategory(unit);
	if(category != -1){
		////L("removing unit");
		//L("IdleUnitRemove(): " << unit);
		IdleUnits[category] -> remove(unit);
		if(category == CAT_BUILDER)
		{
			BuilderTracker* builderTracker = GetBuilderTracker(unit);
			builderTracker -> idleStartFrame = -1; // Its not in the idle list now
			if(builderTracker -> commandOrderPushFrame == -2)
			{
				// bad
				////L("bad");
			}
			
			builderTracker -> commandOrderPushFrame = ai -> cb -> GetCurrentFrame(); // Update the order start frame...
			//assert(builderTracker -> buildTaskId == 0);
			//assert(builderTracker -> taskPlanId == 0);
			//assert(builderTracker -> factoryId == 0);
		}
		////L("removed from list");
		list<integer2>::iterator tempunit;
		bool foundit = false;
		for(list<integer2>::iterator i = Limbo.begin(); i != Limbo.end(); i++){
			if(i -> x == unit){
				tempunit = i;
				foundit=true;
				//L("foundit=true;");
			}
		}
		if(foundit)
			Limbo.erase(tempunit);
		////L("removed from limbo");
	}
}

int CUnitHandler::GetIU(int category) {
	assert((category >= 0) && (category < LASTCATEGORY));
	assert(IdleUnits[category] -> size() > 0);

	// L("GetIU(int category): " << IdleUnits[category] -> front());
	return IdleUnits[category] -> front();
}

int CUnitHandler::NumIdleUnits(int category) {
	//for(list<int>::iterator i = IdleUnits[category] -> begin(); i != IdleUnits[category] -> end();i++)
		////L("Idle Unit: " << *i);
	assert(category >= 0 && category < LASTCATEGORY);
	IdleUnits[category] -> sort();
	IdleUnits[category] -> unique();
	return IdleUnits[category] -> size();
}

void CUnitHandler::MMakerAdd(int unit) {
	metalMaker -> Add(unit);
}
void CUnitHandler::MMakerRemove(int unit) {
	metalMaker -> Remove(unit);
}

void CUnitHandler::MMakerUpdate() {
//	std::cout << "CUnitHandler::MMakerUpdate()" << std::endl;
	metalMaker -> Update();
}


void CUnitHandler::BuildTaskCreate(int id) {
	const UnitDef* newUnitDef = ai -> cb -> GetUnitDef(id);
	int category = ai -> ut -> GetCategory(id);
	float3 pos = ai -> cb -> GetUnitPos(id);
	if((!newUnitDef -> movedata  || category == CAT_DEFENCE) && !newUnitDef -> canfly && category != -1){ // This thing need to change, so that it can make more stuff
		
		// TODO: Hack fix
		if(category == -1)
			return;
		assert(category >= 0);
		assert(category < LASTCATEGORY);
		
		BuildTask bt;
		bt.id = -1;
		//int killplan;
		//list<TaskPlan>::iterator killplan;
		redo:
		for(list<TaskPlan>::iterator i = TaskPlans[category] -> begin(); i != TaskPlans[category] -> end(); i++){
			if(pos.distance2D(i -> pos) < 1 && newUnitDef == i -> def){
				assert(bt.id == -1); // There can not be more than one TaskPlan that is found;
				bt.category = category;
				bt.id = id;
				bt.pos = i -> pos;
				bt.def = newUnitDef;
				list<BuilderTracker*> moveList;
				for(list<BuilderTracker*>::iterator builder = i -> builderTrackers.begin(); builder != i -> builderTrackers.end(); builder++) {
					moveList.push_back(*builder);
					//L("Marking builder " << (*builder) -> builderID << " for removal, from plan " << i -> def -> humanName);
				}
				for(list<BuilderTracker*>::iterator builder = moveList.begin(); builder != moveList.end(); builder++) {
					TaskPlanRemove(*builder);
					BuildTaskAddBuilder(&bt, *builder);
				}
				//bt.builders.push_back(i -> builder);
				//killplan = i -> builder;
				// This plan is gone now
				// Test it by redoing all:
				goto redo; // This is a temp
			}
		}
		if(bt.id == -1){
			//L("*******BuildTask Creation Error!*********");
			// This can happen.
			// Either a builder manges to restart a dead building, or a human have taken control...
			// Make a BuildTask anyway
			bt.category = category;
			bt.id = id;
			if(category == CAT_DEFENCE)
				ai -> dm -> AddDefense(pos,newUnitDef);
			bt.pos = pos;
			bt.def = newUnitDef;
			char text[512];
			sprintf(text, "BuildTask Creation Error: %i", id);
			AIHCAddMapPoint amp;
			amp.label = text;
			amp.pos = pos;
			////ai -> cb -> HandleCommand(&amp);
			// Try to find workers that nearby:
			int num = BuilderTrackers.size();
			
			if(num == 0)
			{
				// Well what now ??? 
				//L("Didnt find any friendly builders");
			} else
			{
				// Iterate over the list and find the builders
				for(list<BuilderTracker*>::iterator i = BuilderTrackers.begin(); i != BuilderTrackers.end(); i++)
				{
					BuilderTracker* builderTracker = *i;
					// Now take a look, and see what its doing:
					const CCommandQueue* mycommands = ai -> cb -> GetCurrentUnitCommands(builderTracker -> builderID);
					if(mycommands -> size() > 0)
					{
						// It have orders
						Command c = mycommands -> front();
						//L("builder: " << builderTracker -> builderID);
						//L("c.id: " << c.id);
						//L("c.params[0]: " <<  c.params[0]);
						if( (c.id == -newUnitDef -> id && c.params[0] == pos.x && c.params[2] == pos.z) // Its at this pos
							|| (c.id == CMD_REPAIR  && c.params[0] == id)  // Its at this unit (id)
							|| (c.id == CMD_GUARD  && c.params[0] == id) ) // Its at this unit (id)
						{
							// Its making this unit
							// Remove the unit from its current job:
							bool hit = false;
							if(builderTracker -> buildTaskId != 0)
							{
								// Hmm, why is this builder idle ???
//								bool hit = true;
								BuildTask* buildTask = GetBuildTask(builderTracker -> buildTaskId);
								if(buildTask -> builderTrackers.size() > 1)
								{
									BuildTaskRemove(builderTracker);
								}
								else
								{
									// This is the only builder of this thing, and now its idle...
									BuildTaskRemove(builderTracker); // IS this smart at all ???
								}
							}
							if(builderTracker -> taskPlanId != 0)
							{
								assert(!hit);
//								bool hit = true;
								// Hmm, why is this builder idle ???
								// 
//								TaskPlan* taskPlan = GetTaskPlan(builderTracker -> taskPlanId);
								TaskPlanRemove(builderTracker);
							}
							if(builderTracker -> factoryId != 0)
							{
								assert(!hit);
//								bool hit = true;
								FactoryBuilderRemove(builderTracker);
							}
							if(builderTracker -> customOrderId != 0)
							{
								assert(!hit);
//								bool hit = true;
								builderTracker -> customOrderId = 0;
							}
							// This builder is now free.
							if(builderTracker -> idleStartFrame == -2)
								IdleUnitRemove(builderTracker -> builderID); // It was in the idle list
							// Add it to this task
							//L("Added builder " << builderTracker -> builderID << " to this new unit buildTask");
							BuildTaskAddBuilder(&bt, builderTracker);
							sprintf(text, "Added builder %i: to buildTaskId: %i (human order?)", builderTracker -> builderID, builderTracker -> buildTaskId);
							AIHCAddMapPoint amp2;
							amp2.label = text;
							amp2.pos = ai -> cb -> GetUnitPos(builderTracker -> builderID);
							////ai -> cb -> HandleCommand(&amp2);
						} else
						{
							// This builder have other orders.
						}
						
					} else
					{
						// This builder is without orders (idle)
					}
					
				}
			
			}
			// Add the task anyway
			BuildTasks[category] -> push_back(bt);
			
		}
		else{
			if(category == CAT_DEFENCE)
				ai -> dm -> AddDefense(pos,newUnitDef);
			BuildTasks[category] -> push_back(bt);
			//TaskPlanRemove(*killplan); // fix
		}
	}
}
void CUnitHandler::BuildTaskRemove(int id)
{
	//L("BuildTaskRemove start");
	int category = ai -> ut -> GetCategory(id);
	// TODO: Hack fix
	if(category == -1)
		return;
	assert(category >= 0);
	assert(category < LASTCATEGORY);
	
	if(category != -1){
		list<BuildTask>::iterator killtask;
		bool found = false;
		//list<list<BuildTask>::iterator> killList;
		for(list<BuildTask>::iterator i = BuildTasks[category] -> begin(); i != BuildTasks[category] -> end(); i++){
			if(i -> id == id){
				killtask = i;
				assert(!found);
				//killList.push_front(i);
				found = true;
			}
		}
		if(found)
		{
			//for(list<list<BuildTask>::iterator>::iterator i = killList.begin(); i != killList.end(); i++){
			//	BuildTasks[category] -> erase(*i);
			//}
			
			// Remove the builders from this BuildTask:
			list<BuilderTracker*> removeList;
			for(list<BuilderTracker*>::iterator builder = killtask -> builderTrackers.begin(); builder != killtask -> builderTrackers.end(); builder++) {
				removeList.push_back(*builder);
				//L("Marking builder " << (*builder) -> builderID << " for removal, from task " << killtask -> id);
			}
			for(list<BuilderTracker*>::iterator builder = removeList.begin(); builder != removeList.end(); builder++) {
				BuildTaskRemove(*builder);
			}
			//assert(false);
			BuildTasks[category] -> erase(killtask);
		}
	}
	//L("BuildTaskRemove end");
}

BuilderTracker* CUnitHandler::GetBuilderTracker(int builder)
{
	for(list<BuilderTracker*>::iterator i = BuilderTrackers.begin(); i != BuilderTrackers.end(); i++){
		if((*i) -> builderID == builder)
		{
			return (*i);
		}
	}
	// This better not happen
	assert(false);
	return 0;
}

void CUnitHandler::BuildTaskRemove(BuilderTracker* builderTracker)
{
	if(builderTracker -> buildTaskId == 0)
	{
		assert(false);
		return;
	}
	int category = ai -> ut -> GetCategory(builderTracker -> buildTaskId);
	// TODO: Hack fix
	if(category == -1)
		return;
	assert(category >= 0);
	assert(category < LASTCATEGORY);
	assert(builderTracker -> buildTaskId != 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	//list<BuildTask>::iterator killtask;
	bool found = false;
	bool found2 = false;
	for(list<BuildTask>::iterator i = BuildTasks[category] -> begin(); i != BuildTasks[category] -> end(); i++){
		if(i -> id == builderTracker -> buildTaskId){
			//killtask = i;
			assert(!found);
			for(list<int>::iterator builder = i -> builders.begin(); builder != i -> builders.end(); builder++){
				if(builderTracker -> builderID == *builder)
				{
					assert(!found2);
					i -> builders.erase(builder);
					builderTracker -> buildTaskId = 0;
					found2 = true;
					break;
				}
			}
			for(list<BuilderTracker*>::iterator builder = i -> builderTrackers.begin(); builder != i -> builderTrackers.end(); builder++){
				
				if(builderTracker == *builder)
				{
					assert(!found);
					i -> builderTrackers.erase(builder);
					builderTracker -> buildTaskId = 0;
					builderTracker -> commandOrderPushFrame = ai -> cb -> GetCurrentFrame(); // Give it time to change command
					found = true;
					break;
				}
			}
			
		}
	}
	assert(found);
	//if(found)
	//	for(list<list<BuildTask>::iterator>::iterator i = killList.begin(); i != killList.end(); i++){
	//		BuildTasks[category] -> erase(*i);
	//	}
	//BuildTasks[category] -> erase(killtask);
}

void CUnitHandler::BuildTaskAddBuilder(BuildTask* buildTask, BuilderTracker* builderTracker)
{
	buildTask -> builders.push_back(builderTracker -> builderID);
	buildTask -> builderTrackers.push_back(builderTracker);
	buildTask -> currentBuildPower += ai -> cb -> GetUnitDef(builderTracker -> builderID) -> buildSpeed;
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	builderTracker -> buildTaskId = buildTask -> id;
}

BuildTask* CUnitHandler::GetBuildTask(int buildTaskId)
{
	for(int k = 0; k < LASTCATEGORY;k++)
	{
		for(list<BuildTask>::iterator i = BuildTasks[k] -> begin(); i != BuildTasks[k] -> end(); i++){
			if(i -> id == buildTaskId)
				return  &*i;
		}
	}
	// This better not happen
	assert(false);
	return 0;
}

BuildTask* CUnitHandler::BuildTaskExist(float3 pos,const UnitDef* builtdef)
{
	int category = ai -> ut -> GetCategory(builtdef);
	// TODO: Hack fix
	if(category == -1)
		return false;
	assert(category >= 0);
	assert(category < LASTCATEGORY);
		
	
	for(list<BuildTask>::iterator i = BuildTasks[category] -> begin(); i != BuildTasks[category] -> end(); i++){
		if(i -> pos.distance2D(pos) < 1 && ai -> ut -> GetCategory(i -> def) == category){
			return &*i; // Hack
		}
	}
	return NULL;
}

bool CUnitHandler::BuildTaskAddBuilder (int builder, int category)
{
	//L("BuildTaskAddBuilder: " << builder);
	assert(category >= 0);
	assert(category < LASTCATEGORY);
	assert(builder >= 0);
	assert(ai -> MyUnits[builder] !=  NULL);
	BuilderTracker * builderTracker = GetBuilderTracker(builder);
	// Make shure this builder is free:
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	
	// See if there are any BuildTasks that it can join
	if(BuildTasks[category] -> size()){
		float largestime = 0;
		list<BuildTask>::iterator besttask;
		for(list<BuildTask>::iterator i = BuildTasks[category] -> begin(); i != BuildTasks[category] -> end(); i++){
			float timebuilding = ai -> math -> ETT(*i) - ai -> math -> ETA(builder,ai -> cb -> GetUnitPos(i -> id));
			if(timebuilding > largestime){
				largestime = timebuilding;
				besttask = i;
			}
		}
		if(largestime > 0){
			BuildTaskAddBuilder(&*besttask, builderTracker);
			ai -> MyUnits[builder] -> Repair(besttask -> id);
			return true;
		}
	}
	// HACK^2    Korgothe...   this thing dont exist...
	if(TaskPlans[category] -> size())
	{
			//L("TaskPlans[category] -> size()");
			float largestime = 0;
			list<TaskPlan>::iterator besttask;
			int units[5000];
			//redo:
			for(list<TaskPlan>::iterator i = TaskPlans[category] -> begin(); i != TaskPlans[category] -> end(); i++){
				float timebuilding = (i -> def -> buildTime / i -> currentBuildPower ) - ai -> math -> ETA(builder,i -> pos);
				
				////L("timebuilding: " << timebuilding << " of " << i -> def -> humanName);
				// Must test if this builder can make this unit/building too
				if(timebuilding > largestime){
					const UnitDef *buildeDef = ai -> cb -> GetUnitDef(builder);
					vector<int> * canBuildList = &ai -> ut -> unittypearray[buildeDef -> id].canBuildList;
					int size = canBuildList -> size();
					int thisBuildingID = i -> def -> id;
					//bool canBuild = false; // Not needed, 
					for(int j = 0; j < size; j++)
					{
						if(canBuildList -> at(j) == thisBuildingID)
						{
							//canBuild = true;
							largestime = timebuilding;
							besttask = i;
							break;
						}
					}
				}
				
				
				int num = ai -> cb -> GetFriendlyUnits(units, i -> pos,200); //returns all friendly units within radius from pos
				for(int j = 0; j < num; j++)
				{
					////L("Found unit at spot");
					
					if((ai -> cb -> GetUnitDef(units[j]) == i -> def) && (ai -> cb -> GetUnitPos(units[j]).distance2D(i -> pos)) < 1 )
					{
						// HACK !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
						//L("if((ai -> cb -> GetUnitDef(units[j]) == i -> def) && (ai -> cb -> GetUnitPos(units[j]).distance2D(i -> pos)) < 1 )");
						//L("TODO: Kill this TaskPlan -- this is BAD -- its on a spot where a building is");
						// TODO: Kill this TaskPlan
						// But not here... as that will mess up the iterator
						
						
						//assert(false);
						//TaskPlans[category] -> erase(i);
						//largestime = 0;
						//goto redo;
					}
				}
				
			}
			//L("largestime: " << largestime);
			
			if(largestime > 10){
				//L("joining the building of " << besttask -> def -> humanName);
				
				assert(builder >= 0);
				assert(ai -> MyUnits[builder] !=  NULL);
				// This is bad. as ai -> MyUnits[builder] -> Build use TaskPlanCreate()
				// It will work however 
				ai -> MyUnits[builder] -> Build(besttask -> pos, besttask -> def);
				return true;
		}
	}
	
	
	return false;
}

void  CUnitHandler::TaskPlanCreate(int builder, float3 pos, const UnitDef* builtdef)
{
	int category = ai -> ut -> GetCategory(builtdef);
	// TODO: Temp hack
	if(category == -1)
		return;
	assert(category >= 0);
	assert(category < LASTCATEGORY);
	
	// Find this builder:
	BuilderTracker * builderTracker = GetBuilderTracker(builder);
	// Make shure this builder is free:
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	
	bool existingtp = false;
	for(list<TaskPlan>::iterator i = TaskPlans[category] -> begin(); i != TaskPlans[category] -> end(); i++){
		if(pos.distance2D(i -> pos) < 20 && builtdef == i -> def){
			assert(!existingtp); // Make shure there are no other TaskPlan there
			existingtp = true;
			TaskPlanAdd(&*i, builderTracker);
		}
	}
	if(!existingtp){
		TaskPlan tp;
		tp.pos = pos;
		tp.def = builtdef;
		tp.currentBuildPower = 0;
		tp.id = taskPlanCounter++;
		TaskPlanAdd(&tp, builderTracker);
		
		if(category == CAT_DEFENCE)
				ai -> dm -> AddDefense(pos,builtdef);
		TaskPlans[category] -> push_back(tp);
	}
}

void CUnitHandler::TaskPlanAdd (TaskPlan* taskPlan, BuilderTracker* builderTracker)
{
	taskPlan -> builders.push_back(builderTracker -> builderID);
	taskPlan -> builderTrackers.push_back(builderTracker);
	taskPlan -> currentBuildPower += ai -> cb -> GetUnitDef(builderTracker -> builderID) -> buildSpeed;
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	builderTracker -> taskPlanId = taskPlan -> id;
}

void CUnitHandler::TaskPlanRemove (BuilderTracker* builderTracker)
{
	list<TaskPlan>::iterator killplan;
	list<int>::iterator killBuilder;
	// Make shure this builder is in a TaskPlan:
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId != 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	builderTracker -> taskPlanId = 0;
	int builder = builderTracker -> builderID;
	bool found = false;
	bool found2 = false;
	for(int k = 0; k < LASTCATEGORY;k++)
	{
		for(list<TaskPlan>::iterator i = TaskPlans[k] -> begin(); i != TaskPlans[k] -> end(); i++){
			for(list<int>::iterator j = i -> builders.begin(); j != i -> builders.end(); j++){
				if(*j == builder){
					killplan = i;
					killBuilder = j;
					assert(!found);
					found = true;
					found2 = true;
				}
			}
		}
		if(found2){
			
			
			for(list<BuilderTracker*>::iterator i = killplan -> builderTrackers.begin(); i != killplan -> builderTrackers.end(); i++){
				if(builderTracker == *i)
				{
					//L("Removing builder " << builder << ", from plan " << killplan -> def -> humanName);
					builderTracker -> commandOrderPushFrame = ai -> cb -> GetCurrentFrame(); // Give it time to change command
					killplan -> builderTrackers.erase(i);
					break;
				}
			}
			
			killplan -> builders.erase(killBuilder);
			if(killplan -> builders.size() == 0)
			{
				//L("The TaskPlans lost all its workers, removeing it");
				if(ai -> ut -> GetCategory(killplan -> def) == CAT_DEFENCE)  // Removeing this is ok ?
					ai -> dm -> RemoveDefense(killplan -> pos,killplan -> def);
				TaskPlans[k] -> erase(killplan);

			}
			found2 = false;
			//break;
		}
		else
		{
			
		}
	}
	if(!found)
	{
		//L("Failed to removing builder " << builder);
		assert(false);
	}
}

TaskPlan* CUnitHandler::GetTaskPlan(int taskPlanId)
{
	for(int k = 0; k < LASTCATEGORY;k++)
	{
		for(list<TaskPlan>::iterator i = TaskPlans[k] -> begin(); i != TaskPlans[k] -> end(); i++){
			if(i -> id == taskPlanId)
				return  &*i;
		}
	}
	// This better not happen
	assert(false);
	return 0;
}

/*
Not used ??
*/
bool CUnitHandler::TaskPlanExist(float3 pos,const UnitDef* builtdef)
{
	int category = ai -> ut -> GetCategory(builtdef);
	// TODO: Hack fix
	if(category == -1)
		return false;
	assert(category >= 0);
	assert(category < LASTCATEGORY);
		
	
	for(list<TaskPlan>::iterator i = TaskPlans[category] -> begin(); i != TaskPlans[category] -> end(); i++){
		if(i -> pos.distance2D(pos) < 100 && ai -> ut -> GetCategory(i -> def) == category){
			return true;
		}
	}
	return false;
}

/*
Add a new factory 
*/
void CUnitHandler::FactoryAdd(int factory)
{
	if(ai -> ut -> GetCategory(factory) == CAT_FACTORY){
		Factory addfact;
		addfact.id = factory;
		Factories.push_back(addfact);
	} else
	{
		assert(false);
	}
}

void CUnitHandler::FactoryRemove(int id) {
	list<Factory>::iterator killfactory;
	bool factoryfound;

	for (list<Factory>::iterator i = Factories.begin(); i != Factories.end(); i++) {
		if (i -> id == id) {
			killfactory = i;
			factoryfound = true;
		}
	}
	if (factoryfound) {
		// remove all builders from this plan
		list<BuilderTracker*> builderTrackers = killfactory -> supportBuilderTrackers;

		for (list<BuilderTracker*>::iterator i = builderTrackers.begin(); i != builderTrackers.end(); i++) {
			FactoryBuilderRemove(*i);
		}

		Factories.erase(killfactory);
	}
}


bool CUnitHandler::FactoryBuilderAdd(int builder) {
	BuilderTracker* builderTracker = GetBuilderTracker(builder);
	return FactoryBuilderAdd(builderTracker);
}

bool CUnitHandler::FactoryBuilderAdd(BuilderTracker* builderTracker) {
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);

	for (list<Factory>::iterator i = Factories.begin(); i != Factories.end(); i++) {
		// don't build-assist hubs
		if ((ai -> MyUnits[i -> id]) -> isHub)
			continue;

		float totalbuildercost = 0;
		// HACK
		for (list<int>::iterator j = i -> supportbuilders.begin(); j != i -> supportbuilders.end(); j++) {
			totalbuildercost += ai -> math -> GetUnitCost(*j);
		}

		if (totalbuildercost < ai -> math -> GetUnitCost(i -> id) * BUILDERFACTORYCOSTRATIO) {
			builderTracker -> factoryId = i -> id;
			i -> supportbuilders.push_back(builderTracker -> builderID);
			i -> supportBuilderTrackers.push_back(builderTracker);
			ai -> MyUnits[builderTracker -> builderID] -> Guard(i -> id);
			return true;
		}
	}

	return false;
}

void CUnitHandler::FactoryBuilderRemove(BuilderTracker* builderTracker) {
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId != 0);
	assert(builderTracker -> customOrderId == 0);
	list<Factory>::iterator killfactory;

	for (list<Factory>::iterator i = Factories.begin(); i != Factories.end(); i++) {
		if (builderTracker -> factoryId == i -> id) {
			i -> supportbuilders.remove(builderTracker -> builderID);
			i -> supportBuilderTrackers.remove(builderTracker);
			builderTracker -> factoryId = 0;
			// give it time to change command
			builderTracker -> commandOrderPushFrame = ai -> cb -> GetCurrentFrame();
		}
	}
}

void CUnitHandler::BuilderReclaimOrder(int builderId, float3 pos) {
	pos = pos;
	BuilderTracker* builderTracker = GetBuilderTracker(builderId);
	assert(builderTracker -> buildTaskId == 0);
	assert(builderTracker -> taskPlanId == 0);
	assert(builderTracker -> factoryId == 0);
	assert(builderTracker -> customOrderId == 0);
	// Just use taskPlanCounter for the id.
	builderTracker -> customOrderId = taskPlanCounter++;
	
}
