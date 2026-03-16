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

nod retea[5];					// retea cu 5 noduri

unsigned char STARE_COM = 0;		// starea initiala a FSA COM
unsigned char TIP_NOD	= 0;		  // tip nod initial: Slave sau No JET
unsigned char STARE_IO 	= 0;		// stare interfata IO - asteptare comenzi
unsigned char ADR_MASTER;				// adresa nod master - numai MS

extern unsigned char AFISARE;		// flag permitere afisare

//***********************************************************************************************************
void TxMesaj(unsigned char i);	// transmisie mesaj destinat nodului i
unsigned char RxMesaj(unsigned char i);		// primire mesaj de la nodul i

//***********************************************************************************************************
void main (void) {
	unsigned char i;
	unsigned char	found = 0;								// variabile locale
	
	WDT_Disable();												// dezactiveaza WDT
	SYSCLK_Init();											  // initializeaza si selecteaza oscilatorul ales in osc.h
	UART1_Init(NINE_BIT, BAUDRATE_COM);		// initilizare UART1 - port comunicatie (TxD la P1.0 si RxD la P1.1)
	UART1_TxRxEN (1,1);									  // validare Tx si Rx UART1
	
 	PORT_Init ();													// conecteaza perifericele la pini (UART0, UART1) si stabileste tipul pinilor

	LCD_Init();    												// 2 linii, display ON, cursor OFF, pozitia initiala (0,0)
	KEYB_Init();													// initializare driver tastatura matriciala locala
	UART0_Init(EIGHT_BIT, BAUDRATE_IO);		// initializare UART0  - conectata la USB-UART (P0.0 si P0.1)

	Timer0_Init();  								      // initializare Timer 0

 	EA = 1;                         			// valideaza intreruperile
 	SFRPAGE = LEGACY_PAGE;          			// selecteaza pagina 0 SFR
	
	for(i = 0; i < NR_NODURI; i++){		    // initializare structuri de date
		retea[i].full = 0;						      // initializeaza buffer gol pentru toate nodurile
		retea[i].bufasc[0] = ':';				    // pune primul caracter in buffer-ele ASCII ":"
	}

	Afisare_meniu();			   				// Afiseaza meniul de comenzi
	
 	while(1){												// bucla infinita
																
		switch(STARE_COM){
			case 0:

#if(PROTOCOL == JT)	// nodul nu detine jetonul, asteapta un mesaj util sau jetonul
				
				switch(RxMesaj(ADR_NOD)){																	// asteapta jetonul de la master
					case TMO:
										Error("Jeton regenerat.");											// anunta ca nodul a regenerat jetonul 
										TIP_NOD = JETON;																			// nodul curent detine jetonul
										if(AFISARE) Afisare_meniu();													// Daca e permisa afisarea, afiseaza meniul de comenzi
										STARE_COM = 1;																				// trece in starea 1
										break;
					case ROK: 																						// a primit un mesaj USER_MES, il afiseaza
										//if(retea[i].bufbin.tipmes == USER_MES)	
											Afisare_mesaj();	
										break;									
					case JOK:																							// a primit un jetonul	
										//if(retea[i].bufbin.tipmes == USER_MES)
										{
											Delay(WAIT/2);																				// asteapta WAIT/2 ms
					
											retea[ADR_NOD].bufbin.adresa_hw_dest = retea[ADR_NOD].bufbin.adresa_hw_src;					// adresa HW dest este adr_hw_src
											retea[ADR_NOD].bufbin.adresa_hw_src	= ADR_NOD;								// adresa HW src este ADR_NOD
											retea[ADR_NOD].bufbin.tipmes = JET_MES;												// tip mesaj = JET_MES
											TxMesaj(ADR_NOD);																			// transmite mesajul JET_MES din bufferul ADR_NOD
											TIP_NOD = JETON;																			// nodul curent detine jetonul
										}
										if(AFISARE) 																					// Daca e permisa afisarea, afiseaza meniul de comenzi		
										{
											Afisare_meniu();
										}					
										STARE_COM = 1;																				// trece in starea 1
										break; 																								// nodul detine jetonul, poate trece sa transmita un mesaj de date

					case CAN:									Error("Msg incomplet.");												break;		// afiseaza eroare Mesaj incomplet
					case CAN_adresa_hw_src:		Error("Adresa sursa lipsa."); 									break;		// afiseaza eroare Mesaj incomplet (adresa_hw_src)
					case CAN_tipmes:					Error("Msg incomplet.");											break;		// afiseaza eroare Mesaj incomplet (tip mesaj)
					case CAN_sc:							Error("SC lipsa.");							 	 						break;		// afiseaza eroare Mesaj incomplet (sc)
					case ESC:	 								Error("Eroare SC.");														break;		// afiseaza Eroare SC
					case TIP:		Error("Tip msg necunoscut!");	// afiseaza Tip mesaj necunoscut

					default:									Error("Cod eroare necunoscut."); Delay(1000); break;		// afiseaza cod eroare necunoscut, apoi asteapta 1 sec
				}
				break;
#endif								
#if(PROTOCOL == MS)		// nodul este slave, asteapta mesaj complet si corect de la master	
				// asteapta un mesaj de la master
				switch(RxMesaj(ADR_NOD))
				{							
					case TMO:	
						// anunta ca nodul curent devine master
						Error("\n\rNod SLAVE -> MASTER!");  
						// nodul curent devine master
						TIP_NOD = MASTER;				
						// Afiseaza meniul de comenzi						
						Afisare_meniu(); 
						// trece in starea 2										
						STARE_COM = 2;	
						// primul slave va fi cel care urmeaza dupa noul master										
						i = ADR_NOD;												
						break;

					case ROK:		// a primit un mesaj de la master, il afiseaza si trebuie sa raspunda
						STARE_COM = 1; 
						Afisare_mesaj(); 
						break;	
					
					case POK:	
						STARE_COM = 1; 	
						break;		
					
					case CAN:	
						Error("\n\rMesaj incomplet!"); // afiseaza eroare Mesaj incomplet
						break;
					
					case TIP:	
						Error("\n\rMesaj necunoscut!"); // afiseaza eroare Tip mesaj necunoscut
						break;	
					
					case ESC:									
						Error("\n\rEroare SC!"); // afiseaza Eroare SC
						break;	
					
					default:	
						Error("\n\rErr necunoscuta!"); // afiseaza cod eroare necunoscut, asteapta 1 sec
						break;	
				}
				break;
#endif
			case 1:											

#if(PROTOCOL == JT)							// nodul detine jetonul, poate transmite un mesaj USER_MES				
				
						found = 0;
						for(i = 0; i<NR_NODURI; i++)																			// cauta sa gaseasca un mesaj util de transmis
						{
							if(retea[i].full==1)
							{
								found = 1;
								break;
							}
						}
							if(found)																												// daca gaseste un mesaj de transmis catre nodul i va incerca sa transmita jetonul nodului urmator 
							{							
								retea[i].bufbin.adresa_hw_dest = retea[i].bufbin.dest;				// adresa HW dest este dest
								TxMesaj(i);																										// transmite mesajul catre nodul i
								//Delay(WAIT/2);																								// asteapta procesarea mesajului la destinatie (WAIT/2 msec)																						
							}
						
						STARE_COM = 2;																										// trece in starea 2, sa transmita jetonul urmatorului nod
						i = ADR_NOD;
#endif	
#if(PROTOCOL == MS)
				// cauta sa gaseasca un mesaj util de transmis
				found = 0;  
				for(i = 0; i < NR_NODURI; i++){
					if(retea[i].full == 1){
							found = 1;
							break;
					}
				}																
				
				if(found) // daca gaseste un nod i
				{  
					// pune adresa HW dest este ADR_MASTER
					retea[i].bufbin.adresa_hw_dest = ADR_MASTER; 
					// transmite mesajul catre nodul i
					TxMesaj(i); 
				}
				else // daca nu gaseste, construieste un mesaj in bufferul ADR_MASTER
				{  
					// adresa HW dest este ADR_MASTER
					retea[ADR_MASTER].bufbin.adresa_hw_dest = ADR_MASTER;
					// adresa HW src este ADR_NOD 
					retea[ADR_MASTER].bufbin.adresa_hw_src = ADR_NOD;	
					// tip mesaj = POLL_MES
					retea[ADR_MASTER].bufbin.tipmes = POLL_MES;		
					// transmite mesajul din bufferul ADR_MASTER
					TxMesaj(ADR_MASTER);  
				}

				// trece in starea 0, sa astepte un nou mesaj de la master
				STARE_COM = 0;				
				break;
#endif						
			break;	
				
			case 2:											// tratare stare 2 si comutare stare

#if(PROTOCOL == JT)											// nodul transmite jetonul urmatorului nod 		
				
				do{
					i++;																															// selecteaza urmatorul slave (incrementeaza i)
					if(i == NR_NODURI) i=0;
				}while(i == ADR_NOD);
						
				Delay(WAIT/2);																										// asteapta WAIT/2 sec
				
				retea[i].bufbin.adresa_hw_dest = i;																// adresa HW dest este i
				retea[i].bufbin.adresa_hw_src	= ADR_NOD;													// adresa HW src este ADR_NOD
				retea[i].bufbin.tipmes = JET_MES;																	// tip mesaj = JET_MES
				TxMesaj(i);																												// transmite mesajul din bufferul i
				STARE_COM = 3;																										// trece in starea 3, sa astepte confirmarea de la nodul i ca jetonul a fost primit
#endif
#if(PROTOCOL == MS)			
			
				// nodul este master, tratare stare 2 si comutare stare
				do {
					// selecteaza urmatorul slave (incrementeaza i)
					i = (++i) % NR_NODURI;	
				} while(i == ADR_NOD);

				// adresa HW dest este i	
				retea[i].bufbin.adresa_hw_dest = i;	

				// daca in bufferul i se afla un mesaj util, il transmite										
				if(retea[i].full == 1)  
				{
					TxMesaj(i);									
				}
				else  // altfel, construieste un mesaj de interogare in bufferul i
				{	
					// adresa HW src este ADR_NOD
					retea[i].bufbin.adresa_hw_src = ADR_NOD; 
					// tip mesaj = POLL_MES
					retea[i].bufbin.tipmes = POLL_MES;      
					// transmite mesajul din bufferul i
					TxMesaj(i);
				}				

				// trece in starea 3, sa astepte raspunsul de la slave-ul i						
				STARE_COM = 3; 
				break;
#endif

			break;

			case 3:
		
#if(PROTOCOL == JT)					
				switch(RxMesaj(ADR_NOD)){									// asteapta un raspuns de la nod i
					case TMO:	Error("\n\rTimeout nod ");		// afiseaza eroare Timeout nod i							
										if(AFISARE)
										{
											UART0_Putch(i+'0');
											LCD_Putch(i+'0');
										}
										STARE_COM = 2;										// revine in starea 2 (nu a primit raspuns)
										break;
					case JOK:	STARE_COM = 0;								// a primit confirmarea transferului jetonului, revine in starea 0
										TIP_NOD = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE) Afisare_meniu();	// Daca e permisa afisarea, afiseaza meniul de comenzi		
										break;
					case ERI:	Error("Err Incadrare!");			// afiseaza Eroare incadrare
										STARE_COM = 0;								// revine in starea 0
										TIP_NOD = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE) Afisare_meniu();	// afiseaza meniul
										break;			
					case ERA:	Error("Err Adresa!");					// afiseaza Eroare adresa
										STARE_COM = 0;								// revine in starea 0
										TIP_NOD = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE) Afisare_meniu();	// afiseaza meniul
										break;								
					case CAN: Error("Msg Incomplet!");			// afiseaza Mesaj incomplet
										STARE_COM	 = 0;								// revine in starea 0
										TIP_NOD = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE) Afisare_meniu();	// afiseaza meniul
										break;							
					case TIP:	Error("Tip msg necunoscut!");	// afiseaza Tip mesaj necunoscut
										STARE_COM = 0;								// revine in starea 0
										TIP_NOD = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE)	Afisare_meniu();	// afiseaza meniul
										break;								
					case ESC:	Error("Eroare SC.");						// afiseaza Eroare SC
										STARE_COM = 0;								// revine in starea 0
										TIP_NOD	 = NOJET;							// nodul nu mai detine jetonul
										if(AFISARE)	Afisare_meniu();  // afiseaza meniul
										break;			
					default: Error("Err noecunoscuta!");			// afiseaza Eroare necunoscuta
									 Delay(1000);										// asteapta 1000 ms
									 STARE_COM = 0;									// revine in starea 0
									 TIP_NOD	= NOJET;							// nodul nu mai detine jetonul
									 if(AFISARE)	Afisare_meniu();	// afiseaza meniul
									 break;			
			}
				
#endif
#if(PROTOCOL == MS)	
				// nodul este slave, asteapta mesaj de la master	
				switch(RxMesaj(i)) // asteapta un raspuns de la slave i
				{						
					case TMO:	
						// afiseaza eroare Timeout nod i
						Error("\n\rTimeout nod ");
						if (AFISARE)
						{
							UART0_Putch(i+'0');
							LCD_Putch(i+'0');
						}
						break;

					case ROK:	Afisare_mesaj(); break;	// a primit un mesaj, il afiseaza
					case POK:	break;							

					case ERI:	
						// afiseaza Eroare incadrare
						Error("\n\rEroare incadrare!");
						break;	

					case ERA: 
						// afiseaza Eroare adresa
						Error("\n\rEroare adresa!");
						break;	

					case TIP:
						// afiseaza Tip mesaj necunoscut
						Error("\n\Mesaj necunoscut!");
						break;	

					case OVR:	
						// afiseaza Eroare suprapunere
						Error("\n\rEroare suprapunere!");
						break;	

					case ESC:
						// afiseaza Eroare SC
						Error("\n\rEroare SC!");
						break;	

					case CAN:
						// afiseaza mesaj incomplet
						Error("\n\rMesaj incomplet!");
						break;	

					case TEST:
						// afiseaza Eroare TEST
						Error("\n\rMesaj incomplet!");
						break;				

					default:
						// afiseaza Eroare necunoscuta, apoi asteapta 1000ms
						Error("\n\rErr necunoscuta!");
						break;	
				}
				// revine in starea 2 (a primit sau nu un raspuns de la slave, trece la urmatorul slave)
				STARE_COM = 2;
				break;	
#endif
			
			break;			
		}
		
		UserIO();							// apel functie interfata
	}
}

