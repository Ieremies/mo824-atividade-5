#include "TS.hpp"

TabuSearch::TabuSearch(Instance* instance, MethodLS methodls,  MethodTS methodTS, int timeLimit, int maxIterations):
  instance(instance), methodls(methodls), methodTS(methodTS), timeLimit(timeLimit), maxIterations(maxIterations), rng(0),
  iterationsInSol(instance->n), fixed(instance->n){}

void TabuSearch::gotoBestNeighborhood(){
  Move bestMove(-1, -1, -1, -1);
  ll bestDelta = INT_MIN;

  FOR(i, sol->n){
    if(fixed[i])
      continue;
    Move curMove = sol->used[i] ? Move(-1, -1, i, -1) : Move(i, -1, -1, -1);

    if(!sol->used[i] && !sol->canAdd(i))
      continue;

    ll delta = sol->deltaAdd(i);
    if(sol->used[i])
      delta *= -1;

    if (tabuSet.count(curMove) && delta + sol->cost <= bestSol->cost)
      continue;

    if (delta > bestDelta){
      bestDelta = delta;
      bestMove = curMove;
      if (bestDelta > 0 && methodls == FirstImprovement)
        break;
    }
  }

  if(methodls != FirstImprovement || bestDelta <= 0){
    vi vin, vout;
    FOR(i, sol->n){
      if(fixed[i])
        continue;
      (sol->used[i] ? vin : vout).push_back(i);
    }
    
    for(int i : vout){
      for(int j : vin){

        if(sol->weight + sol->instance->weights[i] - sol->instance->weights[j] > sol->instance->W)
          continue;

        ll delta = sol->deltaAdd(i) - sol->deltaAdd(j) - sol->instance->cost[i][j] - sol->instance->cost[j][i];
        Move curMove(i, -1, j, -1);
        if (tabuSet.count(curMove) && delta + sol->cost <= bestSol->cost)
          continue;

        if (delta > bestDelta){
          bestDelta = delta;
          bestMove = {i, -1, j, -1};
          if (bestDelta > 0 && methodls == FirstImprovement)
            break;
        }
      }
    }
  }

  if(methodTS == Probabilistic && (methodls != FirstImprovement || bestDelta <= 0)){
    vi vin, vout;
    FOR(i, sol->n){
      if(fixed[i])
        continue;
      (sol->used[i] ? vin : vout).push_back(i);
    }
    
    vi vin1 = vin, vin2 = vin;
    vi vout1 = vout, vout2 = vout;
    shuffle(all(vin1), rng);
    shuffle(all(vin2), rng);
    shuffle(all(vout1), rng);
    shuffle(all(vout2), rng);

    int MAXSZ = 10;
    vin1.resize(min(SZ(vin1), MAXSZ));
    vin2.resize(min(SZ(vin2), MAXSZ));
    vout1.resize(min(SZ(vout1), MAXSZ));
    vout2.resize(min(SZ(vout2), MAXSZ));

    for(int i : vout1){
      for(int h : vout2){
        if(i == h)
          continue;
        for(int j : vin1){
          for(int l : vin2){
            if(j == l)
              continue;

            if(sol->weight + sol->instance->weights[i] + sol->instance->weights[h] - sol->instance->weights[j] - sol->instance->weights[l] > sol->instance->W)
              continue;

            ll delta = sol->deltaAdd(i) + sol->deltaAdd(h) - sol->deltaAdd(j) - sol->deltaAdd(l);
            delta -= sol->instance->cost[i][j] + sol->instance->cost[j][i] + sol->instance->cost[h][j] + sol->instance->cost[j][h];
            delta -= sol->instance->cost[i][l] + sol->instance->cost[l][i] + sol->instance->cost[h][l] + sol->instance->cost[l][h];
            delta += sol->instance->cost[i][h] + sol->instance->cost[h][i];
            delta += sol->instance->cost[j][l] + sol->instance->cost[l][j];
            Move curMove(i, h, j, l);
            if (tabuSet.count(curMove) && delta + sol->cost <= bestSol->cost)
              continue;

            if (delta > bestDelta){
              bestDelta = delta;
              bestMove = {i, h, j, l};
              if (bestDelta > 0 && methodls == FirstImprovement)
                break;
            }
          }
        }
      }
    }
  }

  assert(bestMove.in1 != -1 || bestMove.out1 != -1);
  ll newCost = sol->cost + bestDelta;

  if(bestMove.out1 != -1)
    sol->remove(bestMove.out1);
  if(bestMove.out2 != -1)
    sol->remove(bestMove.out2);
  if(bestMove.in1 != -1)
    sol->add(bestMove.in1);
  if(bestMove.in2 != -1)
    sol->add(bestMove.in2);
  assert(newCost == sol->cost);

  assert(!tabuSet.count(bestMove) || sol->cost > bestSol->cost);
  
  Move revBestMove = Move::Reverse(bestMove);
  if(!tabuSet.count(revBestMove)){
    tabuList.push_back(revBestMove);
    tabuSet.insert(revBestMove);

    if(SZ(tabuList) > maxTabuSize){
      tabuSet.erase(tabuList.front());
      tabuList.pop_front();
    }
  }
  assert(SZ(tabuList) == SZ(tabuSet));

  if(sol->cost > bestSol->cost){
    delete bestSol;
    bestSol = new Solution(*sol);
  }
}

Solution* TabuSearch::run(Solution* sol){
  begin = chrono::steady_clock::now();
  this->sol = new Solution(*sol);
  this->bestSol = new Solution(*sol);

  localSearch();

  delete this->sol;
  bestSol->elapsedTime = getTime();
  bestSol->iterations = lsIterations;

  return bestSol;
}

int TabuSearch::getTime(){
  auto now = chrono::steady_clock::now();
  auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - begin);
  return elapsed.count();
}

void TabuSearch::localSearch(){
  lsIterations = 0;
  const int C1 = 10;
  const int C2 = 30;
  int iterationsToReset = maxIterations / C1;
  int iterationsToUnfix = C2;

  while(true){
    assert(iterationsToReset >= 2 * iterationsToUnfix);
    FOR(i, sol->n)
      if(sol->used[i])
        iterationsInSol[i]++;
      else
        iterationsInSol[i] = 0;

    int elapsedTime = getTime();
    if(elapsedTime > timeLimit || lsIterations >= maxIterations)
      break;

    if(methodTS == Diversification){
      if(iterationsToReset == 0){
        delete sol;
        sol = new Solution(*bestSol);

        int maxVal = *max_element(all(iterationsInSol));
        int maxGet = ceil(maxVal * 0.1);
        FOR(i, sol->n)
          if(iterationsInSol[i] <= maxGet && sol->used[i])
            fixed[i] = true;
        iterationsToReset = maxIterations / C1;
        iterationsToUnfix = C2;
      }

      if(iterationsToUnfix == 0)
        FOR(i, sol->n)
          fixed[i] = 0;
      iterationsToReset--;
      iterationsToUnfix--;
    }

    gotoBestNeighborhood();
    lsIterations++;
  }
}
