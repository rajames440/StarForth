#include "symbolic/polarities.h"

Tpol
DAG_polarity(TDAG DAG)
{
	Tpol polarity = POL_POS;

	while (DAG_symb(DAG) == CONNECTOR_NOT) {
		DAG = DAG_arg0(DAG);
		polarity = INV_POL(polarity);
	}

	return polarity;
}
