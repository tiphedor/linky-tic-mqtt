/***********************************************************************
               Objet decodeur de teleinformation client (TIC)
               format Linky "historique" ou anciens compteurs
               electroniques.

Lit les trames et decode les groupes :
  BASE  : (base) index general compteur en Wh,
  IINST : (iinst) intensit� instantan�e en A,
  PAPP  : (papp) puissance apparente en VA.

Reference : ERDF-NOI-CPT_54E V1

V06 : MicroQuettas mars 2018

***********************************************************************/

/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC.h"

//#define LINKYDEBUG true

/***********************************************************************
                  Objet r�cepteur TIC Linky historique

 Trame historique :
  - d�limiteurs de trame :     <STX> trame <ETX>
    <STX> = 0x02
    <ETX> = 0x03

  - groupes dans une trame : <LF>lmnopqrs<SP>123456789012<SP>C<CR>
    <LR> = 0x0A
    lmnopqrs = label 8 char max
    <SP> = 0x20 ou 0x09 (<HT>)
    123456789012 = data 12 char max
    C = checksum 1 char
    <CR> = 0x0d
       Longueur max : label + data = 7 + 9 = 16

  _FR : flag register

    |  7  |  6  |   5  |   4  |   3  |  2  |   1  |   0  |
    |     |     | _Dec | _GId | _Cks |     | _RxB | _Rec |

     _Rec : receiving
     _RxB : receive in buffer B, decode in buffer A
     _GId : Group identification
     _Dec : decode data

  _DNFR : data available flags

    |  7  |  6  |  5  |  4  |  3  |   2   |   1    |   0   |
    |     |     |     |     |     | _papp | _iinst | _base |

  Exemple of group :
       <LF>lmnopqr<SP>123456789<SP>C<CR>
           0123456  7 890123456  7 8  9
     Cks:  xxxxxxx  x xxxxxxxxx    ^
                                   |  iCks
    Minimum length group  :
           <LF>lmno<SP>12<SP>C<CR>
               0123  4 56  7 8  9
     Cks:      xxxx  x xx    ^
                             |  iCks

    The storing stops at CRC (included), ie a max of 19 chars

***********************************************************************/

/****************************** Macros ********************************/
#ifndef SetBits
#define SetBits(Data, Mask) \
Data |= Mask
#endif

#ifndef ResetBits
#define ResetBits(Data, Mask) \
Data &= ~Mask
#endif

#ifndef InsertBits
#define InsertBits(Data, Mask, Value) \
Data &= ~Mask;\
Data |= (Value & Mask)
#endif

#ifndef P1
#define P1(name) const char name[] PROGMEM
#endif

/***************************** Defines ********************************/
#define CLy_Bds 1200      /* Transmission speed in bds */

#define bLy_Rec   0x01     /* Receiving */
#define bLy_RxB   0x02     /* Receive in buffer B */
#define bLy_Cks   0x08     /* Check Cks */
#define bLy_GId   0x10     /* Group identification */
#define bLy_Dec   0x20     /* Decode */

#define bLy_base  0x01     /* BASE group available */
#define bLy_iinst 0x02     /* IINST group availble */
#define bLy_papp  0x04     /* PAPP group available */

#define Car_SP    0x20     /* Char space */
#define Car_HT    0x09     /* Horizontal tabulation */

#define CLy_MinLg  8      /* Minimum usefull message length */

const char CLy_Sep[] = {Car_SP, Car_HT, '\0'};  /* Separators */

/************************* Donn�es en progmem *************************/
P1(PLy_base)  = "BASE";    /* Rank 0 = CLy_base */
P1(PLy_iinst) = "IINST";   /* Rank 1 = CLy_iinst */
P1(PLy_papp)  = "PAPP";    /* Rank 2 = CLy_papp */

/*************** Constructor, methods and properties ******************/
LinkyHistTIC::LinkyHistTIC(uint8_t pin_Rx, uint8_t pin_Tx) \
      : _LRx (pin_Rx, pin_Tx)  /* Software serial constructor
                                * Achtung : special syntax */
  {
  _FR = 0;
  _DNFR = 0;
  _pRec = _BfA;    /* Receive in A */
  _pDec = _BfB;    /* Decode in B */
  _iRec = 0;
  _iCks = 0;
  _GId = CLy_base;
  _pin_Rx = pin_Rx;
  _pin_Tx = pin_Tx;
  };

void LinkyHistTIC::Init()
  {
  /* Initialise the SoftwareSerial */
  pinMode (_pin_Rx, INPUT);
  pinMode (_pin_Tx, OUTPUT);
  _LRx.begin(CLy_Bds);
  }

void LinkyHistTIC::Update()
  {   /* Called from the main loop */
  char c;
  uint8_t cks, i;
  uint32_t ba;
  uint16_t pa;
  bool Run = true;

  /* Achtung : actions are in the reverse order to prevent
   *           execution of all actions in the same turn of
   *           the loop */

  /* 1st part, last action : decode information */
  if (_FR & bLy_Dec)
    {
    ResetBits(_FR, bLy_Dec);     /* Clear requesting flag */
    _pDec = strtok(NULL, CLy_Sep);

    switch (_GId)
      {
      case CLy_base:
        ba = atol(_pDec);
        if (_base != ba)
          {  /* New value for _base */
          _base = ba;
          SetBits(_DNFR, bLy_base);
          }
        break;

      case CLy_iinst:
        i = (uint8_t) atoi(_pDec);
        if (_iinst != i)
          {  /* New value for _iinst */
          _iinst = i;
          SetBits(_DNFR, bLy_iinst);
          }
        break;

      case CLy_papp:
        pa = atoi(_pDec);
        if (_papp != pa)
          {  /* New value for papp */
          _papp = pa;
          SetBits(_DNFR, bLy_papp);
          }
        break;

      default:
        break;
      }
    }

  /* 2nd part, second action : group identification */
  if (_FR & bLy_GId)
    {
    ResetBits(_FR, bLy_GId);   /* Clear requesting flag */
    _pDec = strtok(_pDec, CLy_Sep);

    if (strcmp_P(_pDec, PLy_base) == 0)
      {
      Run = false;
      _GId = CLy_base;
      }

    if (Run && (strcmp_P(_pDec, PLy_iinst) == 0))
      {
      Run = false;
      _GId = CLy_iinst;
      }

    if (Run && (strcmp_P(_pDec, PLy_papp) == 0))
      {
      Run = false;
      _GId = CLy_papp;
      }

    if (!Run)
      {
      SetBits(_FR, bLy_Dec);   /* Next = decode */
      }
    }

  /* 3rd part, first action : check cks */
  if (_FR & bLy_Cks)
    {
    ResetBits(_FR, bLy_Cks);   /* Clear requesting flag */
    cks = 0;
    if (_iCks >= CLy_MinLg)
      {   /* Message is long enough */
      for (i = 0; i < _iCks - 1; i++)
        {
        cks += *(_pDec + i);
        }
      cks = (cks & 0x3f) + Car_SP;

      #ifdef LINKYDEBUG
      Serial << _pDec << endl;
      #endif

      if (cks == *(_pDec + _iCks))
        {  /* Cks is correct */
        *(_pDec + _iCks-1) = '\0';
                       /* Terminate the string just before the Cks */
        SetBits(_FR, bLy_GId);  /* Next step, group identification */

        #ifdef LINKYDEBUG
        }
        else
        {
        i = *(_pDec + _iCks);
        Serial << F("Error Cks ") << cks << F(" - ") << i << endl;
        #endif
        }   /* Else, Cks error, do nothing */

      }     /* Message too short, do nothing */
    }

  /* 4th part, receiver processing */
  while (_LRx.available())
    {  /* At least 1 char has been received */
    c = _LRx.read() & 0x7f;   /* Read char, exclude parity */

    if (_FR & bLy_Rec)
      {  /* On going reception */
      if (c == '\r')
        {   /* Received end of group char */
        ResetBits(_FR, bLy_Rec);   /* Receiving complete */
        SetBits(_FR, bLy_Cks);     /* Next check Cks */
        _iCks = _iRec-1;           /* Index of Cks in the message */
        *(_pRec + _iRec) = '\0';   /* Terminate the string */

        /* Swap reception and decode buffers */
        if (_FR & bLy_RxB)
          {  /* Receiving in B, Decode in A, swap */
          ResetBits(_FR, bLy_RxB);
          _pRec = _BfA;       /* --> Receive in A */
          _pDec = _BfB;       /* --> Decode in B */
          }
          else
          {  /* Receiving in A, Decode in B, swap */
          SetBits(_FR, bLy_RxB);
          _pRec = _BfB;     /* --> Receive in B */
          _pDec = _BfA;     /* --> Decode in A */
          }

        }  /* End reception complete */
        else
        {  /* Other character */
        *(_pRec+_iRec) = c;   /* Store received character */
        _iRec += 1;
        if (_iRec >= CLy_BfSz-1)
          {  /* Buffer overrun */
          ResetBits(_FR, bLy_Rec); /* Stop reception and do nothing */
          }
        }  /* End other character than '\r' */
      }    /* End on-going reception */
      else
      {    /* Reception not yet started */
      if (c == '\n')
        {   /* Received start of group char */
        _iRec = 0;
        SetBits(_FR, bLy_Rec);   /* Start reception */
        }
      }
    }  /* End while */
  }


bool LinkyHistTIC::baseIsNew()
  {
  bool Res = false;

  if(_DNFR & bLy_base)
    {
    Res = true;
    ResetBits(_DNFR, bLy_base);
    }
  return Res;
  }

bool LinkyHistTIC::iinstIsNew()
  {
  bool Res = false;

  if(_DNFR & bLy_iinst)
    {
    Res = true;
    ResetBits(_DNFR, bLy_iinst);
    }
  return Res;
  }

bool LinkyHistTIC::pappIsNew()
  {
  bool Res = false;

  if(_DNFR & bLy_papp)
    {
    Res = true;
    ResetBits(_DNFR, bLy_papp);
    }
  return Res;
  }

uint32_t LinkyHistTIC::base()
  {
  return _base;
  }

uint8_t LinkyHistTIC::iinst()
  {
  return _iinst;
  }

uint16_t LinkyHistTIC::papp()
  {
  return _papp;
  }

/***********************************************************************
               Fin d'objet r�cepteur TIC Linky historique
***********************************************************************/
