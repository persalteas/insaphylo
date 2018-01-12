
#include "Sampler.h"

double Sampler::GetLogProb()  {
  // Bayes theorem : p( T,u | D ) prop. p( D | T,u ).p(T,u)
  // ln p(D,T,U) = ln p(D | T,u) + ln p(T,u) 
  return GetLogLikelihood() + GetLogPrior();
}

double Sampler::GetLogPrior()	{
  // p(T,u) = p(T).p(u)
  // ln p(T,u) = ln p(T) + ln p(u)
  // ln p(T,u) = 0       + ln exp(-u)
	return -rate;
}

double Sampler::GetLogLikelihood()	{
	double logL{};
	for (int i{GetNsite()}; i--;)
		logL += SiteLogLikelihood(i);
	return logL;
}

double Sampler::SiteLogLikelihood(int site)	{
  // L = sum( 1/4 * L(0,i) )
  double L = 0;
  Pruning(site, GetRoot());
  for (int i=0; i<4; i++)
    L += (GetRoot()->condl[i])/4.0;
	return log(L);
}

void Sampler::Pruning(int site, Node* node)	{

  // store the 4 likelihoods in attribute node.condl (double[4])

  if (! node->left) // if we are on a leaf, read the data
  {
    int state = data->GetState(node->GetNodeName(), site);
    if (state == -1) // indel in alignment
    {
      for (int j=0; j<4; j++)
        node->condl[j] = 1.0;
    }
    else // only one possibility is allowed
      node->condl[state] = 1.0;
  }
  else 
  {
    double lhs, rhs, v, q, p;

    // update the sons likelihoods
    Pruning(site, node->left); 
    Pruning(site, node->right);

    for (int sk = 0; sk < 4; sk++)
    {
      // Left computation
      lhs = 0;
      v = node->left->GetBranchLength();
      q = exp(- rate * v);
      p = 1 - q;
      for(int i = 0; i < 4; i++)
        lhs += ( q * (i==sk) + p * 1/4) * node->left->condl[i];

      // Right computation
      rhs = 0;
      v = node->right->GetBranchLength();
      q = exp(- rate * v);
      p = 1 - q;
      for(int j = 0; j < 4; j++)
        rhs += ( q * (j==sk) + p * 1/4) * node->right->condl[j];

      node->condl[sk] = lhs * rhs;
    }
  }
}

int Sampler::RateMove(double tuning)	{
  rate = rate + tuning*(Random::Uniform()-0.5);
	if (rate < 0) rate = -rate;
	return 0;
}

int Sampler::TimeMove(double tuning)	{
  tree->ProposeTimeMove(tuning);
	return 0;
}

int Sampler::TopoMove()	{
  tree->ProposeSPRMove();
	return 0;
}

void Sampler::Cycle()	{
// - Proposition de changement: (T,u) -> (T*,u*)
//     - Time-Move: Changer la date d'un noeud
//     - Topo-Move: Changer la topologie de l'arbre (décrocher une branche, la greffer ailleurs à la même date) = mouvement de SPR: Subtree pruning and regrafting
//     - Rate-Move: Changer le taux de mutation u ( this->rate )
// - Calculer le rapport alpha = p(T*,u* | D) / p(T,u | D)
// - Accepter le changement avec un proba min(1, alpha)


  int move_type = Random::Choose(3);
  double bkrate = rate;

  // backup of (T,u)
  if (move_type < 2) tree->Backup();
  double logpost1 = GetLogProb();

  // propose (T*,u*)
  switch (move_type)
  {
    case (0): TopoMove(); break;
    case (1): TimeMove(0.1); break;
    case (2): RateMove(0.1); 
  }
  double logpost2 = GetLogProb();

  // compute ratio of final/initial probs
  double logalpha = logpost2 - logpost1;

  // decide wether accept or not
  if (log(Random::Uniform()) >= logalpha) 
  {
    // the move is rejected
    if (move_type < 2) tree->Restore();
    else rate = bkrate;
  }
}


