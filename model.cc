/***
*	autor: Michal Kysilko
*	datum: listopad 2013	
*	tema: volebni infrastruktura CR
*
*	vsechny casy jsou v sekundach
***/

#include <simlib.h>
#include <iostream> 
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>

#define POCET_CLENU_KOMISE 6
#define AGREGACNI_NASOBITEL 31
#define POCET_MIST_ZA_PLENTOU 1
#define POCET_URN 1
#define POCET_OKRSKU 200
#define POCET_OSRP 53//53
#define POCET_KRAJU 14//14
#define KONEC_HLASOVANI 54000


class Sit {
	public:
		unsigned long pocetOkrsku;
		unsigned long pocetOsrp;
		unsigned long pocetKraju;
};

//okrsek - sho, ve kterem se provadi voleni do urny a pocitaji se hlasy
class Okrsek {
	public: 
		unsigned long id;
		std::string nazev;
		unsigned long nadrazenaOblast;
		bool zacloPocitani;
		unsigned long pVolicu;
		unsigned long pHlasu;
		unsigned long spocitanychHlasu;
		unsigned long pPrichozich;
		Okrsek() {
			pVolicu = 0;
			pHlasu = 0;	
			pPrichozich = 0;
			zacloPocitani = false;		
		}	
};

//Obec s rozsirenou pusobnosti prijima zpravy od okrsku, zpracovava a posisla je na kraj
class Osrp {
	public:
		unsigned long id;
		std::string nazev;
		unsigned long nadrazenaOblast;
		unsigned long pVolicu;
		unsigned long pocetPodrazenychOblasti;
		unsigned long zpracovanychOkrsku;
		Osrp() {
			pVolicu = 0;
			pocetPodrazenychOblasti = 0;
			zpracovanychOkrsku = 0;			
		}		

};


//Kraj prijima vysledky od osrp, zpracovava je a posila je na centralu
class Kraj {	
	public: 
		unsigned long id;
		std::string nazev;
		unsigned long nadrazenaOblast;
		unsigned long pocetPodrazenychOblasti;
		unsigned long pVolicu;
		unsigned long zpracovanychOsrp;
		Kraj() {
			pVolicu = 0;
			pocetPodrazenychOblasti = 0;
			zpracovanychOsrp = 0;			
		}
};

Sit Sit;

//TODO: pridelat do site parametry a tohle resit dynamicteji
Okrsek poleOkrsky[200];	//v siti je 200 okrsku
Osrp poleOsrp[53];	//v siti je 53 obci s rozsirenou pusobnosti
Kraj poleKraje[14]; //v siti je 14 kraju

Facility osrpFac[POCET_OSRP];
Facility krajFac[POCET_KRAJU];
Facility centralaFac;

Facility urnaPristup[POCET_URN *AGREGACNI_NASOBITEL*POCET_OKRSKU];
Facility zaPlentou[POCET_MIST_ZA_PLENTOU * AGREGACNI_NASOBITEL*POCET_OKRSKU];
Facility clenKomise[POCET_CLENU_KOMISE * AGREGACNI_NASOBITEL*POCET_OKRSKU];

Queue 	Qkomise[POCET_OKRSKU], 
		Qplenta[POCET_OKRSKU], 
		Qurna[POCET_OKRSKU], 
		QpocitaniKomise[POCET_OKRSKU], 
		QosrpZprac, 
		QkrajZprac, 
		QcentralaZprac;

 //TODO: statistiky, histogramy, experimenty, ...
Stat statVolicVoli("Celkova doba volice v systemu");
Stat statPocitaniHlasu("Doba pocitani hlasu v okrsku");
TStat statDelkaFrontyKomise("Delka fronty volicu u komise");




double startPocitaniQKomise[POCET_OKRSKU];

//rozparsuje sit ze souboru a ulozi ziskana data do objektu trid Okrsek, Kraj a Osrp
int nactiSit() {
    std::string str, typOblasti, nazevOblasti;
    unsigned long pVolicu;
	std::ifstream soubor ("sit.txt");
	Kraj nadrazenyKraj;
	Osrp nadrazenyOsrp;
	Sit.pocetOkrsku = 0;
	Sit.pocetOsrp = 0;
	Sit.pocetKraju = 0;
	
	unsigned long tempOkrsek = 0;
	unsigned long tempOsrp = 0;
	
	if(soubor.is_open()) {
		while(std::getline(soubor,str)) {			
			if(str == "") continue;				//prazdne radky preskocim
			
			std::size_t pos = str.find(";");	//najdu prvni strednik
			typOblasti = str.substr(0,pos);		//vsechno pred prvnim strednikem oznacim za typ oblasti...
			str = str.substr(pos+1);			//...a smazu ze stringu
			pos = str.find(";");				//najdu dalsi strednik	atd.
			nazevOblasti = str.substr(0,pos);	
			str = str.substr(pos+1);
			pos = str.find(";");
			pVolicu = std::stoi(str.substr(0,pos));
			
			//nastavim hodnoty oblasti podle dat vytazenych ze site
			if(typOblasti == "kraj") {
								
				Kraj *newKraj = new Kraj();			
				newKraj->id = Sit.pocetKraju;			//id kraje
				newKraj->nazev = nazevOblasti;			//nazev kraje
				newKraj->pVolicu = pVolicu;				//celkovy pocet volicu v kraji
							
				nadrazenyKraj = *newKraj;
				poleKraje[Sit.pocetKraju] = *newKraj;	//ulozim kraj do pole
				if(Sit.pocetKraju > 0) poleKraje[Sit.pocetKraju-1].pocetPodrazenychOblasti = tempOsrp;	//ulozim pocet osrp, ktere ma kraj pod sebou
				Sit.pocetKraju++;
				tempOsrp = 0;
							
			} else if(typOblasti == "osrp") {
			
				Osrp *newOsrp = new Osrp();
				newOsrp->id = Sit.pocetOsrp;
				newOsrp->nazev = nazevOblasti;
				newOsrp->pVolicu = pVolicu;
				newOsrp->nadrazenaOblast = nadrazenyKraj.id;	//zapisu id kraje, ktery je nadrazeny teto obci	
					
				if(Sit.pocetOsrp > 0) poleOsrp[Sit.pocetOsrp-1].pocetPodrazenychOblasti = tempOkrsek;	//ulozim si pocet okrsku, ktere ma obec pod sebou
				nadrazenyOsrp = *newOsrp;						//zapamatuji si vzdy posledni obec, abych ji pak mohl pouzit k prirazeni nadrazene obce nad okrskem
				poleOsrp[Sit.pocetOsrp] = *newOsrp; 			//pridam obec do pole
				Sit.pocetOsrp++;
				tempOkrsek = 0;
				tempOsrp++;
				
			} else if(typOblasti == "okrsek") {

				Okrsek *newOkrsek = new Okrsek();
				newOkrsek->id = Sit.pocetOkrsku;
				newOkrsek->nazev = nazevOblasti;
				newOkrsek->pVolicu = pVolicu;
				newOkrsek->nadrazenaOblast = nadrazenyOsrp.id; 	//zapisu id obce, ktera je nadrazena tomuto okrsku
				
				poleOkrsky[Sit.pocetOkrsku] = *newOkrsek;			//ulozim okrsek do pole
				Sit.pocetOkrsku++;							
				tempOkrsek++;
				
			}

		}
		
		poleOsrp[Sit.pocetOsrp-1].pocetPodrazenychOblasti = tempOkrsek;
		poleKraje[Sit.pocetKraju-1].pocetPodrazenychOblasti = tempOsrp;
	}
	soubor.close();
}

int zpracovanychKraju = 0;


//model centraly - prijima zpravy od kraju a chvili je zpracovava
//kdyz zpracuje vsechny kraje, tak se ukonci simulace
class ZpracovaniNaCentrale : public Process {
	void Behavior() {
		zpet:
		if (centralaFac.Busy()) { //pokud je centrala zaneprazdnena, tak si kraj jakoby stoupne do fronty
			Into(QcentralaZprac); 
			Passivate();
			goto zpet;
		} else {
			
			Seize(centralaFac); //kraj zabere centralu
			Wait(Uniform(300,600)); //prekontroluje a pricte hlasy
			Release(centralaFac);
			if (QcentralaZprac.Length()>0) {
				(QcentralaZprac.GetFirst())->Activate();
			}
		}
		zpracovanychKraju++;	
		if(zpracovanychKraju == POCET_KRAJU) {
			Wait(Uniform(600,1200));
			Stop();
		}	
	}
	public: 
		Kraj *actKraj;
		ZpracovaniNaCentrale(Kraj *k){
			actKraj = k;	
		}

};

//krajska komise zpracovava vysledky z obci a posila souhrnne vysledky na centralu
class ZpracovaniNaKraji : public Process {
	void Behavior() {
		zpet:
		if (krajFac[actKraj->id].Busy()) { 
			Into(QkrajZprac);
			Passivate();
			goto zpet;
		}
		
		Seize(krajFac[actKraj->id]);
		Wait(Uniform(300,600)); //komise prekontroluje a secte hlasy
		poleKraje[actKraj->id].zpracovanychOsrp++; //obec zpracovana
		Release(krajFac[actKraj->id]);
		if (QkrajZprac.Length()>0) {
			(QkrajZprac.GetFirst())->Activate();
		}
		
		//mam zpracovane vsechny obce => posilam zpravu na centralu
		if(poleKraje[actKraj->id].zpracovanychOsrp == poleKraje[actKraj->id].pocetPodrazenychOblasti) {
			Wait(Uniform(30,60)); // odeslani vysledku na centralu prostrednictvim internetu
			(new ZpracovaniNaCentrale(actKraj))->Activate();
		}	
		
	}
	public: 
		Kraj *actKraj;
		ZpracovaniNaKraji(Kraj *k){
			actKraj = k;	
		}
};


//obecni komise zpracovava vysledky z okrsku a posila souhrnne vysledky na kraj
class ZpracovaniNaOsrp : public Process {
	void Behavior() {
		zpet:
		if (osrpFac[actOsrp->id].Busy()) { 
			Into(QosrpZprac);
			Passivate();
			goto zpet;
		}
		
		Seize(osrpFac[actOsrp->id]);
		Wait(Uniform(300,600)); //obecni komise prekontroluje a pricte hlasy
		Release(osrpFac[actOsrp->id]);
		poleOsrp[actOsrp->id].zpracovanychOkrsku++;	
		if (QosrpZprac.Length()>0) {
			(QosrpZprac.GetFirst())->Activate();
		}
		
		if(poleOsrp[actOsrp->id].zpracovanychOkrsku == poleOsrp[actOsrp->id].pocetPodrazenychOblasti) {
			Wait(Uniform(30,60)); //odeslani vysledku ke krajske komisi prostrednictvim internetu
			(new ZpracovaniNaKraji(&poleKraje[actOsrp->nadrazenaOblast]))->Activate();
		}		
		
	}
	public: 
		Osrp *actOsrp;
		ZpracovaniNaOsrp(Osrp *o){
			actOsrp = o;	
		}
};


class OdesilaniVysledku : public Event {
	void Behavior(){
		(new ZpracovaniNaOsrp(&poleOsrp[okrsek->nadrazenaOblast]))->Activate();
	}
	public: 
		Okrsek *okrsek;
		OdesilaniVysledku(Okrsek *o){
			okrsek = o;	
		}

};

class PocitaniHlasu : public Process {
	void Behavior() {
		//komisar vezme hlas a pocita
		int kt = -1;
		zpetKomise:
		for (int a=okrsek->id*POCET_CLENU_KOMISE*AGREGACNI_NASOBITEL; a<((okrsek->id+1) *POCET_CLENU_KOMISE * AGREGACNI_NASOBITEL); a++) {
			if (!clenKomise[a].Busy()) { 
				kt=a; break; 
			}
		}
		if (kt == -1) {
			Into(QpocitaniKomise[okrsek->id]);
			Passivate();
			goto zpetKomise;
		}

		Seize(clenKomise[kt]);
		Wait(Uniform(10,20)); //cas potrebny pro zpracovani jednoho hlasu z urny
		okrsek->spocitanychHlasu--;
		Release(clenKomise[kt]);
		if (QpocitaniKomise[okrsek->id].Length()>0) {
			(QpocitaniKomise[okrsek->id].GetFirst())->Activate();
		}
		
		if(okrsek->spocitanychHlasu == 0){
			statPocitaniHlasu(Time-startPocitaniQKomise[okrsek->id]);
			Wait(Uniform(600,3600)); //cesta s flashkou k obecni komisi
			(new OdesilaniVysledku(okrsek))->Activate();
		}					
	}
	public: 
		Okrsek *okrsek;
		PocitaniHlasu(Okrsek *o) {
			okrsek = o;
		}
};

//startuje procesy pocitani hlasu
class GeneratorPocitaniHlasu : public Event {
	void Behavior() {
		startPocitaniQKomise[okrsek->id] = Time;
		(new PocitaniHlasu(okrsek))->Activate();	
		okrsek->pHlasu--;
		if(okrsek->pHlasu == 0) Passivate();
		else Activate();
		
	}
	public: 
		Okrsek *okrsek;
		GeneratorPocitaniHlasu(Okrsek *o) {
			okrsek = o;
		}

};

//proces Volice: jde ke komisi, pak za plentu a nakonec hazi listek do urny
class Volic : public Process {
	void Behavior() {
	
		//interakce s komisi		
		double prichod = Time;
		int kt = -1;
		zpetKomise:
		for (int a=okrsek->id*POCET_CLENU_KOMISE * AGREGACNI_NASOBITEL; a<((okrsek->id+1)*POCET_CLENU_KOMISE * AGREGACNI_NASOBITEL); a++) {
			if (!clenKomise[a].Busy()) { 
				kt=a; break; 
			}
		}
		statDelkaFrontyKomise(Qkomise[okrsek->id].Length());
		if (kt == -1) {
			Into(Qkomise[okrsek->id]);	//skutecne musim resit frontu? 
			Passivate();
			goto zpetKomise;
		}

		Seize(clenKomise[kt]);
		Wait(Uniform(90,300)); //komisar zapisuje, ze volic prisel, dava mu obalku a pokyny
		Release(clenKomise[kt]);
		
		if (Qkomise[okrsek->id].Length()>0) {
			(Qkomise[okrsek->id].GetFirst())->Activate();
		}
		
		//interakce s mistem zaPlentou
		kt = -1;
		zpetPlenta:
		for (int a=okrsek->id*AGREGACNI_NASOBITEL; a<(okrsek->id+1)*AGREGACNI_NASOBITEL; a++) {
			if (!zaPlentou[a].Busy()) { 
				kt=a; break; 
			}
		}
		if (kt == -1) {
			Into(Qplenta[okrsek->id]);
			Passivate();
			goto zpetPlenta;
		}

		Seize(zaPlentou[kt]);
		Wait(Uniform(15,60)); //volic dava listek do obalky v miste "za plentou"
		Release(zaPlentou[kt]);
		if (Qplenta[okrsek->id].Length()>0) {
			(Qplenta[okrsek->id].GetFirst())->Activate();
		}

		//interakce s urnou
		kt = -1;
		zpetUrna:
		for (int a=okrsek->id*AGREGACNI_NASOBITEL; a<(okrsek->id+1)*AGREGACNI_NASOBITEL; a++) {
			if (!urnaPristup[a].Busy()) { 
				kt=a; break; 
			}
		}
		if (kt == -1) {
			Into(Qurna[okrsek->id]);
			Passivate();
			goto zpetUrna;
		}

		Seize(urnaPristup[kt]);
		Wait(Uniform(2,8)); //volic hazi obalku s listkem do urny
		okrsek->pHlasu++;	
		Release(urnaPristup[kt]);
		if (Qurna[okrsek->id].Length()>0) {
			(Qurna[okrsek->id].GetFirst())->Activate();
		}
		statVolicVoli(Time-prichod);
		
		//vsichni volici v okrsku odhlasovali
		if(okrsek->pHlasu == okrsek->pVolicu) {
			okrsek->zacloPocitani = true;
			okrsek->spocitanychHlasu = okrsek->pHlasu;
			(new GeneratorPocitaniHlasu(okrsek))->Activate();
		}
	}
	public:
		Okrsek *okrsek;
		Volic(Okrsek *o){
			okrsek = o;
		} 
};

//generator prichodu volicu do okrsku
class Generator : public Event {
	void Behavior() {
		(new Volic(okrsek))->Activate();
		Activate(Time + Exponential(1.25));
		okrsek->pPrichozich++;
		if(okrsek->pPrichozich == okrsek->pVolicu) {
			Passivate();
		}
	}
	public:
		Okrsek *okrsek;
		Generator(Okrsek *o) {
			okrsek = o;
		}
};


//event konce hlasovani, ktery je naplanovan po 15h od zacatku
//v okamziku konce voleb se zacinaji pocitat hlasy, pokud uz se nepocitaji
class KonecHlasovani : public Event {
	void Behavior() {
		for (int i=0; i<POCET_OKRSKU; i++) {
			Qkomise[i].clear(); 
			Qplenta[i].clear(); 
			Qurna[i].clear();
			if(poleOkrsky[i].zacloPocitani != true) {
				poleOkrsky[i].spocitanychHlasu = poleOkrsky[i].pHlasu;
				(new GeneratorPocitaniHlasu(&poleOkrsky[i]))->Activate();
			}
		}	
	}
};

int main() {	
	
	/****
	* Nacteni site ze souboru
	****/
	nactiSit();
	
	
	/****
	* Inicializace statistik
	****/
	statVolicVoli.Clear();
	statPocitaniHlasu.Clear();
	statDelkaFrontyKomise.Clear();


	/****
	* Simulace
	****/
	Init(0); //Inicializace simulace, zaciname od casu 0	
	(new KonecHlasovani)->Activate(KONEC_HLASOVANI); //event konec hlasovani - po 15 hodinach (8h prvni den a 7h druhy den)	
	for (int i=0; i<POCET_OKRSKU; i++) 
		(new Generator(&poleOkrsky[i]))->Activate();	//aktivace generatoru volicu pro kazdy okrsek
	Run();	//Start simulace
		
	/****
	* Statistiky
	****/
	std::cout << Time << std::endl;
	statVolicVoli.Output();
	statPocitaniHlasu.Output();
	statDelkaFrontyKomise.Output();
}


