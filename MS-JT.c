#include <c8051F040.h>	// declaratii SFR
#include <wdt.h>
#include <osc.h>
#include <port.h>
#include <uart0.h>
#include <uart1.h>
#include <lcd.h>
#include <keyb.h>
#include <Protocol.h>
#include <UserIO.h>

nod retea[NR_NODURI];				// retea cu 3 noduri

unsigned char STARE_COM = 0;		// starea initiala a FSA COM
unsigned char STARE_IO 	= 0;		// stare initiala FSA interfata IO - asteptare comenzi
unsigned char TIP_NOD	= 0;		// tip nod initial: Slave sau No JET
unsigned char ADR_MASTER;			// adresa nod master - numai MS

extern unsigned char AFISARE;		// flag permitere afisare

//***********************************************************************************************************
void TxMesaj(unsigned char i);				// transmisie mesaj destinat nodului i
unsigned char RxMesaj(unsigned char i);		// primire mesaj de la nodul i

//***********************************************************************************************************
void main (void) {
	unsigned char i, found;				// variabile locale
	
	WDT_Disable();							// dezactiveaza WDT
	SYSCLK_Init();							// initializeaza si selecteaza oscilatorul ales in osc.h
	UART1_Init(NINE_BIT, BAUDRATE_COM);		// initilizare UART1 - port comunicatie (TxD la P1.0 si RxD la P1.1)
	UART1_TxRxEN (1,1);						// validare Tx si Rx UART1
	
 	PORT_Init ();							// conecteaza perifericele la pini (UART0, UART1) si stabileste tipul pinilor

	LCD_Init();    							// 2 linii, display ON, cursor OFF, pozitia initiala (0,0)
	KEYB_Init();							// initializare driver tastatura matriciala locala
	UART0_Init(EIGHT_BIT, BAUDRATE_IO);		// initializare UART0  - conectata la USB-UART (P0.0 si P0.1)

	Timer0_Init();  						// initializare Timer 0

 	EA = 1;                         		// valideaza intreruperile
 	SFRPAGE = LEGACY_PAGE;          		// selecteaza pagina 0 SFR
	
	for(i = 0; i < NR_NODURI; i++){		// initializare structuri de date
		retea[i].full = 0;					// initializeaza buffer gol pentru toate nodurile
		retea[i].bufasc[0] = ':';			// pune primul caracter in buffer-ele ASCII ':'
	}

	Afisare_meniu();			   			// Afiseaza meniul de comenzi
	
 	while(1){								// bucla infinita
																
		switch(STARE_COM){
			case 0:

#if(PROTOCOL == MS)							// nodul este slave, asteapta mesaj complet si corect de la master	

				switch(RxMesaj(ADR_NOD)){			// asteapta un mesaj de la master
					case TMO:							// anunta ca nodul curent devine master
						Error("\n\rTMO - Devenit Master");
						TIP_NOD = MASTER;				// nodul curent devine master
						Afisare_meniu();				// Afiseaza meniul de comenzi
						STARE_COM = 2;					// trece in starea 2
						i = ADR_NOD;					// primul slave va fi cel care urmeaza dupa noul master
						break;

					case ROK:	
						Afisare_mesaj();				// a primit un mesaj de la master, il afiseaza
						break;	
					case POK:	
						STARE_COM = 1; 					// si trebuie sa raspunda
						break;					
					case CAN:	
						Error("\n\rMesaj incomplet");	// afiseaza eroare Mesaj incomplet
						break;	
					case TIP:	
						Error("\n\rTip necunoscut");	// afiseaza eroare Tip mesaj necunoscut
						break;	
					case ESC:	
						Error("\n\rEroare SC");			// afiseaza Eroare SC
						break;	
					default:	
						Error("\n\rEroare RX");			// afiseaza cod eroare necunoscut
						// Delay(1000); 				// (Optional) asteapta 1 sec, daca ai o functie Delay implementata
						break;	
				}
#endif							
				break;

			case 1:											

#if(PROTOCOL == MS)										// nodul este slave, transmite mesaj catre master			
				
				found = 0;								// cauta sa gaseasca un mesaj util de transmis
				for(i = 0; i < NR_NODURI; i++) {
					if(retea[i].full) {					// daca gaseste un mesaj util
						TxMesaj(i);						// transmite mesajul pentru nodul i
														// (adresa HW dest este setata catre MASTER in rutinele Tx/UserIO)
						found = 1;
						break;
					}
				}

				if(!found) {							// daca nu gaseste, construieste un mesaj in bufferul ADR_MASTER
					retea[ADR_MASTER].bufbin.src = ADR_NOD;			// adresa HW src este ADR_NOD
					retea[ADR_MASTER].bufbin.tipmes = POLL_MES;		// tip mesaj = POLL_MES
					retea[ADR_MASTER].bufbin.lng = 0;				// lungime date = 0
					TxMesaj(ADR_MASTER);							// transmite mesajul din bufferul ADR_MASTER
				}											

				STARE_COM = 0;							// trece in starea 0, sa astepte un nou mesaj de la master
#endif
				break;	
				
			case 2:											// tratare stare 2 si comutare stare

#if(PROTOCOL == MS)											// nodul este master, tratare stare 2 si comutare stare
															
				i++;										// selecteaza urmatorul slave (incrementeaza i...)
				if(i >= NR_NODURI) i = 0;
				if(i == ADR_NOD) {							// (...si sare peste adresa proprie)
					i++;
					if(i >= NR_NODURI) i = 0;
				}

				if(retea[i].full) {							// daca in bufferul i se afla un mesaj util, il transmite
					TxMesaj(i);
				} else {									// altfel, construieste un mesaj de interogare in bufferul i
					retea[i].bufbin.src = ADR_NOD;			// adresa HW src este ADR_NOD
					retea[i].bufbin.tipmes = POLL_MES;		// tip mesaj = POLL_MES
					retea[i].bufbin.lng = 0;
					TxMesaj(i);								// transmite mesajul din bufferul i
				}
															
				STARE_COM = 3;								// trece in starea 3, sa astepte raspunsul de la slave-ul i
#endif
				break;

			case 3:

#if(PROTOCOL == MS)										// nodul este master, asteapta mesaj de la slave	
																
				switch(RxMesaj(i)){								// asteapta un raspuns de la slave i
					case TMO:										
						Error("\n\rTimeout nod");				// afiseaza eroare Timeout nod i
						break;
					case ROK:	
						Afisare_mesaj();						// a primit un mesaj de date, il afiseaza
						break;									// daca a primit un mesaj de interogare (POK), nu face nimic special
					case POK:
						break;									// a primit raspuns POLL gol de la slave, e OK
					case ERI:	
						Error("\n\rEroare incadrare");			// afiseaza Eroare incadrare
						break;	
					case ERA:	
						Error("\n\rEroare adresa");				// afiseaza Eroare adresa
						break;	
					case TIP:	
						Error("\n\rTip necunoscut");			// afiseaza Tip mesaj necunoscut
						break;	
					case OVR:	
						Error("\n\rEroare suprap.");			// afiseaza Eroare suprapunere
						break;	
					case ESC:	
						Error("\n\rEroare SC");					// afiseaza Eroare SC
						break;	
					case CAN:	
						Error("\n\rMesaj incomplet");			// afiseaza mesaj incomplet
						break;
					default:	
						Error("\n\rEroare necunoscuta");		// afiseaza Eroare necunoscuta
						// Delay(1000);							// asteapta 1000ms
						break;	
				}
				STARE_COM = 2;									// revine in starea 2 (trece la urmatorul slave)
#endif			
				break;			
		}
		
		UserIO();							// apel functie interfata
	}
}
