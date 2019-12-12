/* Player must push four buttons/hall sensors in a specific order to activate a linear actuator
 *  programmable via button for easy setup and change
 *  reset button to close actuator and reset the game

// pin assignments
 * Signal     Pin                
 *            Arduino Uno     
 * ------------------------
 * 0   Serial RX
 * 1   Serial TX
 * 2   SW1 - buttone one    
 * 3   SW2 - buttone two
 * 4   SW3 - buttone three
 * 5   SW4 - buttone four    
 * 6   SW6 - program button
 * 7   SW7 - reset button     
 * 8   RL1 - Relay open button     
 * 9   RL2 - Relay close button
 * 10 ChipSelect for Ethernet
 * 11   SPI MOSI for Ethernet
 * 12   SPI MISO for Ethernet
 * 13   SPI SCK  for Ethernet
 * 14  (A0)     RL2 - option for lasers or LED
 * 15  (A1)     Power LED1
 * 16  (A2)   
 * 17  (A3)   
 * 18  (A4/SDA) 
 * 19  (A5/SCK) 
 
 
 The Ethernet Shield SPI bus (10,11,12,13) with 10 as select
 also uses 4 as select for the SD card on the ethernet shield
 high deselects for the chip selects.  Reserve these incase the
 puzzle is ever connected to via an Ethernet shield.
 
 General operation:
 
  STARTUP
    If a winning pattern is not stored for the game, 
    go to the programming process.  If a winning pattern is stored, 
    go to the reset process.  
  PROGRAMMING
    Capture the order of the button presses for 4 buttons.  When
    the user presses the program button again, store the sequence as 
    the winning sequence and go to the reset routine.
    
    The sequence is stored as the pin numbers of the buttons
    (eg sequence SW1   SW2  SW1   SW3 is stored as 2,3,2,4)
    
  RESET
    Called at startup, after programming and when the reset input is activated
    Close actuator (RL2)
    Clear any in progress game play.
    Go to game play.
  GAME PLAY
      Monitor SW1 - SW4.  Each time a button is pressed, see if it is
      the next button in the winning sequence.  If it is, continue the sequence
      if it is not, reset the sequence buffer.
      
      If the full winning sequence is entered, open the actuator
    
      The game will sit idle until the reset is activated.
      
 */

#include <EEPROM.h>     // winning sequence will be stored in EEProm so it is not lost when power is cycled
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// Constant Values

#define DEBOUNCE_DLY 50       // Number of mS before a switch state is accepted


// I/O Pin Naming

#define   SW1   22     //four buttons for the game
#define   SW2   23     //the code relies on these four buttons
#define   SW3   24     //being adjacent pin numbers

#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)#define   PrgButton  6   //Program button
#define ManualOpen 27       //Manual Override to Complete Game
#define PrgButton 28
#define RstButton  29   //reset button
#define OpenRelay  30   //relay open output
#define CloseRelay 31   //relay close output

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  //create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Variables/Storage

byte      NextButton = 1;     //holds the EEPROM address for the next button in sequence
byte      EEVal = 255;      //used to read the value from the EEPROM

bool      SW1State;
bool      SW2State;
bool      SW3State;
bool      PrgState;
bool      RstState;
bool      CompletionState;

void setup()
{
  // put your setup code here, to run once:
  
  // serial setup
  Serial.begin(9600);
  Serial.println("Serial interface started");
  
  //Pin Setup
  pinMode(SW1,INPUT_PULLUP);
  pinMode(SW2,INPUT_PULLUP);
  pinMode(SW3,INPUT_PULLUP);
  pinMode(PrgButton,INPUT_PULLUP);
  pinMode(RstButton,INPUT_PULLUP);
  pinMode(ManualOpen, INPUT_PULLUP);
  pinMode(OpenRelay,OUTPUT);
  pinMode(CloseRelay, OUTPUT);

    if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));
  
   if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
  }

EEVal = EEPROM.read(NextButton);
  if (EEVal > SW3 || EEVal < SW1)   // if the next button value is invalid, goto programming mode 
  {
    Serial.println("No winning sequence detected, going to program mode");
    RecordSequence();
  }
  
  ResetGame();
  
  
}

void loop() {
  // put your main code here, to run repeatedly:
  //check buttons for the game
    CheckButton(SW1, SW1State);
    CheckButton(SW2, SW2State);
    CheckButton(SW3, SW3State);
    
  if (DebounceSW(PrgButton) !=PrgState)
  { 
    PrgState = !PrgState;
    if (PrgState) { RecordSequence();}
  }
    
  if (DebounceSW(RstButton) != RstState)  
  { 
    RstState = !RstState;
    if (RstState) {ResetGame();}
  }
  if(DebounceSW(ManualOpen) == true)
  {
    CompleteGame();
  }
} //end of loop

void CheckButton(byte SWx, bool& SWState)
{ //check the game buttons.
  //Read the button, 
  //if it is the next in the sequence
  //    advance the sequence
  //    if the sequence is finished, open relay for actuator
  //if it is not the next in sequence,
  //    reset the sequence
  
  bool IsActive;
  IsActive = DebounceSW(SWx);
  
  if (IsActive == SWState) { return;} //if the switch state is the same as it was before, take no action
  
  SWState = IsActive;         //store the new state
  
  if (!IsActive) {  return;}    //if the new state is off - return now
  
  Serial.print("Pin # ");
  Serial.print(SWx);
  Serial.println(" activated.");
  
  //the switch is active
  EEVal = EEPROM.read(NextButton);
  
  if (SWx == EEVal)
  {
    NextButton++;           //increment the button address
    EEVal = EEPROM.read(NextButton);  //read the new value
    
    Serial.print("Next expected pin activation: ");
    Serial.println(EEVal);
    
    if (EEVal == 255)         //if the value is 255, the sequence is over
    {
      CompleteGame();
    } 
    
  } else
  {
    Serial.println("Wrong button pressed, resetting sequence");
    NextButton = 1;
    EEVal = EEPROM.read(NextButton);  //read the new value
    
    Serial.print("Next expected pin activation: ");
    Serial.println(EEVal);
  }
  
} //end of CheckButton

void RecordSequence()
{
  //record a new winning sequence by monitoring the buttons.
  //for each game button pressed, store the value.  Also montor
  //the program button - when it is pressed, write 255 to the next
  //address and exit programming.
  
  Serial.println("Entered programming mode");
  
  NextButton = 1;
  do
  {
    if (DebounceSW(SW1) == true ) { AddButton(SW1);}
    if (DebounceSW(SW2) == true ) { AddButton(SW2);}
    if (DebounceSW(SW3) == true ) { AddButton(SW3);}
    
    if (DebounceSW(PrgButton) == true) 
    {
      AddButton(255);
      NextButton = 11;
    }
    
  } while (NextButton<11);
  
  ResetGame();
  
  
} //end of RecordSequence

void AddButton(byte SWx)
{
  EEPROM.write(NextButton,SWx);
  Serial.print(" Pin # ");
  Serial.print(SWx);
  Serial.println(" added to sequence.");
  NextButton++;
}

void ResetGame()
{
  //Reset the game and get ready for a new player
  
  NextButton = 1;               //reset to the first expected button
  EEVal = EEPROM.read(NextButton);      //read the value
  
  
  digitalWrite(OpenRelay, LOW);  // intiates the door closing
  Serial.println("Actuator Closed");
  
  Serial.println("The game is reset, ready to play!");
  
  Serial.print("Next expected pin activation: ");
  Serial.println(EEVal);
    
} //end of ResetGame

void CompleteGame()
{
  Serial.println("Sequence solved! Opening Actuator!");
  digitalWrite(OpenRelay,HIGH);
  musicPlayer.setVolume(20,20);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  musicPlayer.playFullFile("track001.mp3");
  musicPlayer.playFullFile("track002.mp3");
  Serial.println("Playing track");
  Serial.println("Actuator Open");
    
} //end of CompleteGame

bool DebounceSW(byte SWx)
{
  //read the passed switch twice and make sure both readings match
  //this prevents multiple triggers due to mechanical noise in 
  //the switch
  
  
  bool PossVal2;
  bool PossVal = !digitalRead(SWx);   //invert the reading due to the use of pullups
  
  while(true)
  {
    delay(DEBOUNCE_DLY);          //delay by the debounce amount
    PossVal2 = !digitalRead(SWx);     //re-read the switch
    
    if (PossVal == PossVal2)        //if the two reads are the same
    {
      return (PossVal);         //return the read value
    }
    
    //this code will only execute if the two reads did not match
    //Now read the pin again and look for a match.
    //If the button is cycling very fast, it is possible the code
    //will deadlock here.  This is a very slim possibility
    
    PossVal = !digitalRead(SWx);      //re-take the first reading
    //and loop back to the delay
  }
  
  return (PossVal);   //this line is never executed, but makes the compiler happy.
  
}
