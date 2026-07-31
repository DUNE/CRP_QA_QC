#ifndef AD7746_USB_H
#define AD7746_USB_H

#include "AD7746.h"

#define AD_VENDOR_ID	0x456   /* Analog Devices, Inc */
#define AD_PRODUCT_ID	0xb481	 /* AD7746 EVAL board */

#define ERR_EXIT(errcode) do { printf("   %s\n", libusb_strerror((enum libusb_error)errcode)); return -1; } while (0)
#define CALL_CHECK_CLOSE(fcall, hdl) do { int _r=fcall; if (_r < 0) { libusb_close(hdl); ERR_EXIT(_r); } } while (0)


//#include "cypress_code/Bulkloop_SDCC/fx2regs.h" liste des registres du fx2
#define I2CS	0xE678  // Control & Status
#define I2DAT	0xE679  // Data
#define I2CTL	0xE67A  // I2C Control


#define FX2_CPUCS_ADDR		(0xE600)
#define FX2_INT_RAMSIZE		(0x4000)

#define VENDORCMD_TIMEOUT	(5000)
#define MAX_LINE_LENGTH		(512)
#define MAX_BYTES_PER_LINE	(256)
#define EEPROM_WRITE_SIZE	(1024)

#define CHAR_TO_HEXVAL(c)	((((c) >= '0') && ((c) <= '9')) ? ((c) - '0') : ((((c) - 'A') & 0x0F) + 10))
#define GET_HEX_BYTE(char_p)	(((CHAR_TO_HEXVAL((char_p)[0])) << 4) | (CHAR_TO_HEXVAL((char_p)[1])))
#define GET_HEX_WORD(char_p)	(((CHAR_TO_HEXVAL((char_p)[0])) << 12) | ((CHAR_TO_HEXVAL((char_p)[1])) << 8) | \
((CHAR_TO_HEXVAL((char_p)[2])) << 4) | (CHAR_TO_HEXVAL((char_p)[3])))


#define PORTACFG_ADD		(0xE670)



/*
 * These are the requests (bRequest) that the bootstrap loader is expected
 * to recognize.  The codes are reserved by Cypress, and these values match
 * what EZ-USB hardware, or "Vend_Ax" firmware (2nd stage loader) uses.
 * Cypress' "a3load" is nice because it supports both FX and FX2, although
 * it doesn't have the EEPROM support (subset of "Vend_Ax").
 */
// This is the OpCode argument to ezusb_read or ezusb_write
#define RW_INTERNAL		0xA0		/* hardware implements this one */
#define RW_EEPROM		0xA2
#define RW_MEMORY		0xA3
#define GET_EEPROM_SIZE	0xA5


// facteurs d'extension mesures dans l'ordre des cartes (b501, b502, b503, b504, b505)
// b501: 3.012469 sur cin1,  3.014354 sur cin2
// b502: 2.972332 sur cin1,  2.970137 sur cin2
// b503: 2.998665 sur cin1,  3.000764 sur cin2
// b504: 3.008756 sur cin1,  3.010973 sur cin2
// b505: 2.974319 sur cin1,  2.975767 sur cin2
const double calibratedAD7746_extendedFactor[5]={3.013,2.971,3.000,3.010,2.975};



class AD7746Class{
    int chipRevision;
    double capaGAIN=1.;
    double Cref=4.096;
    double Ccapdac=4.096*3.2; // par construction Ccapdac=Cref*3.2
    double CcapdacStep=Ccapdac/127./*0x7f*/; // !!! attention non lineaire
    double ratioCrefCapdDacC=3.2;  // valeur constructeur!! ne semble pas coller
    int capaOffsetDAC=0;
    double capaOffset=0.;
    double VoltGAIN=1.;
    double IntTempGAIN=1.;
    double IntTempOffset=0.;
    // extended range circuit (voir CN0129.pdf)
    // si on veut avoir un offset ~50 et un range de +-10 il faut un F=3  R2=R1*(F+1)/(F-1) => R2=2*R1
    // possible avec 100kOhm et 200kOhm
    double Fextended=1.;  // F=(R1+R2)/(R1-R2);
    
    unsigned char capReg=0x0; // default value, value will be updated by writeCapSetupRegister
    unsigned char VTReg=0;// default value, value will be updated by writeVtSetupRegister
    unsigned char ConfigReg=0; // default value, value will be updated by writeConfigurationRegister
    unsigned char CapDacAReg=0x0; // default value, value will be updated by writeCapDacARegister
    unsigned char CapDacBReg=0x0; // default value, value will be updated by writeCapDacARegister
    
    bool temperatureRead;
    bool capaciteRead;
    bool ADready4conversion;
    
    int waitInus=1000;
    
    bool singleConversion=0;
    bool conversionStarted=false;
    bool conversionStartedCin1=false;
    bool conversionStartedCin2=false;
    
    void resetConversionStarted(){ conversionStarted=conversionStartedCin1=conversionStartedCin2=false; }
    
    // user fitted correction of offset vs capdac offset= a * capadc + b * capdac^2 + c * capdac^3
    // for cin1 only
    bool userOffsetCorrection=false;
    double userOffsetCorrection_a, userOffsetCorrection_b, userOffsetCorrection_c;
public:
    long identifiant;
    enum enumChannel {cin1,cin2};
    bool enable[2]={false,false};
    char name[2][20]={"",""};
    char orientation[2][20]={"",""};
    double fullCapa[2]={0.}, emptyCapa[2]={0.};
    unsigned int status;
    unsigned int capaDAC;
    unsigned int VTDAC;
    bool outRange;
    
    double temperature;
    double capacite;
    double capaciteCin1=0.;
    double capaciteCin2=0.;
    int capaciteCin1Status=4;
    int capaciteCin2Status=4;
    
    unsigned char calibratedCapaDac_A_cin1=0x0;
    unsigned char calibratedCapaDac_A_cin2=0x0;
    
    void setUserOffsetCorrection( double a, double b=0, double c=0){
        userOffsetCorrection=true;
        userOffsetCorrection_a=a;
        userOffsetCorrection_b=b;
        userOffsetCorrection_c=c;
    }
    
    
    
    AD7746Class(const int ID=0){
        
        int r=0;
        String infoString = "";
        
        // identifier
        long b1=i2c_read_reg( AD7746_RA_CAP_GAIN_H);
        long b2=i2c_read_reg( AD7746_RA_CAP_GAIN_L);
        int b3=i2c_read_reg( AD7746_RA_VOLT_GAIN_H);
        int b4=i2c_read_reg( AD7746_RA_VOLT_GAIN_L);
        identifiant=(b1<<24)|(b2<<16)|(b3<<8)|b4;
        
        readStatusReg();// first read failed???
        writeCapOffsetRegister(0x8000);// valeur par defaut => offset=0.
        
        readChipRevisonRegister(); printf("Chip revision = 0x%x\n",chipRevision);
        readStatusReg();// first read failed???
        readCapOffsetRegister();// capacitive Offset calibration (for offset lower than 1pF)
        readCapGAINRegister();// capacitive gain calibration, factory calibrated
        readVoltGAINRegister();// voltage gain calibration, factory calibrated
        printf("capa OFFset=0x%x %f pF   gain=%f\n",capaOffsetDAC,capaOffset,capaGAIN);
        //	readIntTempGAINRegister();// Internal Temperature gain, NOT IN DOC
        //	printf("INT temp gain=%f\n",IntTempGAIN);
        //	readIntTempOffRegister();// Internal Temperature off, NOT IN DOC
        //	printf("INT temp off=%f\n",IntTempOffset);
        
        printf("relative Range =+-%f pF\n",4.096*Fextended);
        printf("max Range : -%f -- %f pF\n",-4.096*Fextended, Ccapdac*Fextended+4.096*Fextended);
        printf("max shift (Ccapdac)=%f pF,  step=%f  (non lineaire)\n",Ccapdac*Fextended,CcapdacStep*Fextended);
        
        
        
        capReg=readCapSetupRegister(); printf("capReg= 0x%x\n",capReg);
        VTReg=readVtSetupRegister(); printf("VTReg= 0x%x\n",VTReg);
        unsigned char Exc=readExcSetupRegister(); printf("Exc= 0x%x\n",Exc);
        ConfigReg=readConfigurationRegister(); printf("ConfigReg= 0x%x\n",ConfigReg);
        CapDacAReg=readCapDacARegister(); printf("CapDacAReg= 0x%x\n",CapDacAReg);
        CapDacBReg=readCapDacBRegister(); printf("CapDacBReg= 0x%x\n",CapDacBReg);
        singleConversion=false;
        if( (ConfigReg&7) ==2  ) singleConversion=true;
        ADready4conversion=true;
        
    }
    
    ~AD7746Class(){
        printf("Closing device...\n");
    }
    
    // void wait4Ready(){
    //     //readConfigurationRegister();
    //     //if( ((ConfigReg>>1)&0x1)==1 ) printf("probleme!!\n");
    //     while( !ADready4conversion ) {
    //         read_data(false);
    //         delayMicroseconds(waitInus);
    //     }
    // }

    void wait4Ready(){
        unsigned long t0 = millis();
        while (!ADready4conversion) {
            read_data(false);
            if (millis() - t0 > 250) {   // 250 ms timeout
                ADready4conversion = true;   // fail open so you don't brick the loop()
                break;
            }
            delayMicroseconds(waitInus);
        }
    }



    bool isCin1ConversionStarted(){
        if( !ADready4conversion && ( ((capReg>>6)&1)==0 && ((capReg>>7)&1)==1) ) return true; // doit etre 0 pour le cin1 et 1 pour le enable
        return false;
    }
    bool isCin2ConversionStarted(){
        if( !ADready4conversion && ( ((capReg>>6)&1)==1 && ((capReg>>7)&1)==1) ) return true; // doit etre 1 pour le cin2 et 1 pour le enable
        return false;
    }
    
    
    void setChannel(enumChannel i, const char *name_, const char *orientation_){
        enable[i]=true;
        strcpy(name[i],name_);
        strcpy(orientation[i],orientation_);
    }
    void setChannel(enumChannel i, const char *name_, double fullCapa_, double emptyCapa_){
        enable[i]=true;
        strcpy(name[i],name_);
        fullCapa[i]=fullCapa_;
        emptyCapa[i]=emptyCapa_;
    }
    void setRelativeCalibration(enumChannel i, const char *name_, double a, double b){
        fullCapa[i]= a*(fullCapa[i]-emptyCapa[i])+emptyCapa[i]+b;
        emptyCapa[i]+= b ;
    }
    
    double getFillingRatio(enumChannel i){
        return (capacite+getOffset(i) - emptyCapa[i])/(fullCapa[i]-emptyCapa[i]);
    }
    double getCref(){return Cref;}
    double getMaxRange(){return 4.096*Fextended;}
    double getLowerFullRange(){return -4.096*Fextended;}
    double getUpperFullRange(){return Ccapdac*Fextended+4.096*Fextended;}
    double getCapaDacStep(){return CcapdacStep*Fextended;}
    double getCapaDacMax(){return Ccapdac*Fextended;}
    double getFinecapaOffset(){return capaOffset;}
    void setExtensionFactor(double f=0.){
        if( f<0.9 ) {
            f=1.;
            if(identifiant>=0xb501 && identifiant<=0xb505)
                f=calibratedAD7746_extendedFactor[identifiant-0xb501]; // set factor to the calibrated measured value
            else {
                if( identifiant == 0x6266595d ) f=calibratedAD7746_extendedFactor[0xb503-0xb501]; // 0xb503
            }
            printf("Setting calibrated extension factor=%f\n",f);
        }
        Fextended=f;
        if( Fextended>1.){ // setup the EXC channels to apply the extension circuit
            unsigned char Exc=0; Exc |=/*CLKCTRL*/0<<7 | /*EXCON*/ 1<<6 | /*EXCB*/ 1<<5 | /*EXCBbar*/ 0<<4 | /*EXCA*/ 0<<3 | /*EXCAbar*/ 1<<2 | /*ECCLVL1*/ 1<<1 | /*EXCLVL0*/ 1;
            writeExcSetupRegister(Exc);
        }else{  // setup the EXC channels to disable the extension circuit
            unsigned char Exc=0; Exc |=/*CLKCTRL*/0<<7 | /*EXCON*/ 1<<6 | /*EXCB*/ 1<<5 | /*EXCBbar*/ 0<<4 | /*EXCA*/ 1<<3 | /*EXCAbar*/ 0<<2 | /*ECCLVL1*/ 1<<1 | /*EXCLVL0*/ 1;
            writeExcSetupRegister(Exc);
        }
        {int ExcRead=readExcSetupRegister(); printf("ExcRead= 0x%x\n",ExcRead);}
        printf("relative Range =+-%f pF\n",4.096*Fextended);
        printf("max Range : -%f -- %f pF\n",-4.096*Fextended, Ccapdac*Fextended+4.096*Fextended);
        printf("max shift (Ccapdac)=%f pF,  step=%f  (non lineaire)\n",Ccapdac*Fextended,CcapdacStep*Fextended);
    }
    double getExtensionFactor(){return Fextended;}
    double getOffset(enumChannel i){
        double off=0.;
        if(i==cin1) {
            off= calibratedCapaDac_A_cin1*getCapaDacStep();
            if(userOffsetCorrection){
                double x=calibratedCapaDac_A_cin1;
                off= userOffsetCorrection_a *x + userOffsetCorrection_b *x*x + userOffsetCorrection_c *x*x*x;
            }
        }
        if(i==cin2) off= calibratedCapaDac_A_cin2*getCapaDacStep();
        return off;
    }
    double getLowerAbsoluteRange(enumChannel i){return getOffset(i)-4.096*Fextended;}
    double getUpperAbsoluteRange(enumChannel i){return getOffset(i)+4.096*Fextended;}
    
    unsigned char getcalibratedCapaDac_A(enumChannel i){
        if(i==cin1) return calibratedCapaDac_A_cin1;
        if(i==cin2) return calibratedCapaDac_A_cin2;
        return 0;
    }
    
    double getCapGAIN() {return capaGAIN;}
    double setCapGAIN( double gain) {
        capaGAIN= gain;
        Cref=4.096*capaGAIN;
        Ccapdac=Cref*ratioCrefCapdDacC; // par construction du AD7746
        CcapdacStep=Ccapdac/127./*0x7f*/; // !!! attention non lineaire
        printf("set capGain=%f\n",gain);
        printf("max Range : -%f -- %f pF\n",-4.096*Fextended, Ccapdac*Fextended+4.096*Fextended);
        printf("max shift (Ccapdac)=%f pF,  step=%f  (non lineaire)\n",Ccapdac*Fextended,CcapdacStep*Fextended);
        return capaGAIN;
    }
    
    void setDifferential(bool diff=true){
        int readv=readCapSetupRegister();
        if( ((readv>>5) & 0x1) ==0 && diff){
            readv^= 1<<5; // flip bit at 5 /*CAPDIFF*/
            writeCapSetupRegister(readv);
        }
        if( ((readv>>5) & 0x1) ==1 && !diff){
            readv^= 1<<5; // flip bit at 5 /*CAPDIFF*/
            writeCapSetupRegister(readv);
        }
    }
    
    void printByte(int cont){
        printf("content=0x%02x ",cont);
        for(int i=8;i>0;--i) printf(" %1d", (cont>>(i-1))&1);
    }
    void printByte( unsigned char cont){
        printf("content=0x%02x ",cont);
        for(int i=8;i>0;--i) printf(" %1d", (cont>>(i-1))&1);
    }
    
    void dumpRegisters(){
        for(unsigned char reg=0; reg<0x31; reg++){
            int cont=i2c_read_reg( reg);
            printf("reg=0x%02x   ",reg);
            printByte(cont);
            printf("\n");
        }
    }
    
    
    double getAbsoluteCapa(enumChannel icin){
        if(icin==cin1) return getAbsoluteCapaCin1();
        else if(icin==cin2) return getAbsoluteCapaCin2();
        return 0.;
    }
    double getAbsoluteCapaCin1(){
        setCin1();
        get_dataCin1();
        double value=capacite+calibratedCapaDac_A_cin1*getCapaDacStep();
        printf("getAbsoluteCapaCin1  value=%f pF (lecture=%f CAPDDAC : 0x%x : %f pF)\n",value,capacite,calibratedCapaDac_A_cin1,calibratedCapaDac_A_cin1*getCapaDacStep());
        return value;
    }
    double getAbsoluteCapaCin2(){
        setCin2();
        get_dataCin2();
        double value=capacite+calibratedCapaDac_A_cin2*getCapaDacStep();
        printf("getAbsoluteCapaCin2  value=%f pF  CAPDDAC : 0x%x : %f pF,  offset=%f\n",value,calibratedCapaDac_A_cin2,calibratedCapaDac_A_cin2*getCapaDacStep());
        return value;
    }
    
    
    double calibreOffset(bool verbose=false){ // calibre fine OFFset in AD7746 (common to all channel)
        int ConfigRegSave=ConfigReg;
        unsigned char Config=0; Config |=/*VTF1*/0<<7 | /*VFT0*/ 0<<6 | /*CAPF2*/ 1<<5 | /*CAPF1*/ 1<<4 | /*CAPF0*/ 1<<3 | /*MD2*/ 1<<2 | /*MD1*/ 0<<1 | /*MD0*/ 1;
        if(verbose){
            readCapOffsetRegister();// capacitive Offset calibration (for offset lower than 1pF)
            printf("old capa OFFset=0x%x  %f pF\n",capaOffsetDAC,capaOffset);
        }
        int timeout=20;
        int ret=0;
        writeConfigurationRegister(Config);
        do{
            ret=readConfigurationRegister();
            if(verbose) printf("Config write=0x%x  read=0x%x\n",ConfigReg,ret);
            delayMicroseconds(20000);
        }while( ret==Config && timeout--);
        readCapOffsetRegister();// capacitive Offset calibration (for offset lower than 1pF)
        if(verbose) printf("New capa OFFset=0x%x  %f pF\n",capaOffsetDAC,capaOffset);
        writeConfigurationRegister(ConfigRegSave);
        return capaOffset;
    }
    
    void setCapDaC(enumChannel icin, int value){ // in single end only CAPDACA is usefull, the other one is connected on cin- which is not used
        if(icin==cin1) setCapDaC_cin1(value);
        if(icin==cin2) setCapDaC_cin2(value);
    }
    void setCapDaC_cin1(int value){ // in single end only CAPDACA is usefull, the other one is connected on cin- which is not used
        calibratedCapaDac_A_cin1=value;
    }
    void setCapDaC_cin2(int value){ // in single end only CAPDACA is usefull, the other one is connected on cin- which is not used
        calibratedCapaDac_A_cin2=value;
    }
    
    
    void configAD7746_EVAL_ruler(){ // setting verifie avec le soft windows (sauf pour la temperature)
        printf("Setting AD7746-EVAL ruler demo\n");
        //{int capRegRead=readCapSetupRegister(); printf("capRegRead= 0x%x\n",capRegRead);}
        //capReg=0; capReg |=/*CAPEN*/1<<7 | /*CIN2*/ 1<<6 | /*CAPDIFF*/ 0<<5 | /*CAPCHOP*/ 1;
        capReg=0; capReg |=/*CAPEN*/1<<7 | /*CIN2*/ 1<<6 | /*CAPDIFF*/ 0<<5 | /*CAPCHOP*/ 0;  // chop doit etre 0 d'apres la doc??
        writeCapSetupRegister(capReg,true);
        {int capRegRead=readCapSetupRegister(); printf("capRegRead= 0x%x\n",capRegRead);}
        
        //{int VTRead=readVtSetupRegister(); printf("VTRead= 0x%x\n",VTRead);}
        //unsigned char VT=0; VT |=/*VTEN*/1<<7 | /*VTMD1*/ 0<<6 | /*VTMD2*/ 0<<5 | /*EXTREF*/ 0<<4 | /*VTSHORT*/ 0<<1 | /*VTCHOP*/ 1;
        VTReg=0; VTReg |=/*VTEN*/1<<7 | /*VTMD1*/ 0<<6 | /*VTMD2*/ 0<<5 | /*EXTREF*/ 0<<4 | /*VTSHORT*/ 0<<1 | /*VTCHOP*/ 1;
        writeVtSetupRegister(VTReg);
        {int VTRead=readVtSetupRegister(); printf("VTRead= 0x%x\n",VTRead);}
        
        //{int ExcRead=readExcSetupRegister(); printf("ExcRead= 0x%x\n",ExcRead);}
        unsigned char Exc=0; Exc |=/*CLKCTRL*/0<<7 | /*EXCON*/ 1<<6 | /*EXCB*/ 1<<5 | /*EXCBbar*/ 0<<4 | /*EXCA*/ 1<<3 | /*EXCAbar*/ 0<<2 | /*ECCLVL1*/ 1<<1 | /*EXCLVL0*/ 1;
        // test avec 00 pour voir si ca augmente le range !! a priori non
        //unsigned char Exc=0; Exc |=/*CLKCTRL*/0<<7 | /*EXCON*/ 1<<6 | /*EXCB*/ 1<<5 | /*EXCBbar*/ 0<<4 | /*EXCA*/ 1<<3 | /*EXCAbar*/ 0<<2 | /*ECCLVL1*/ 1<<0 | /*EXCLVL0*/ 0;
        writeExcSetupRegister(Exc);
        {int ExcRead=readExcSetupRegister(); printf("ExcRead= 0x%x\n",ExcRead);}
        
        //{int configcRead=readConfigurationRegister(); printf("configcRead= 0x%x\n",configcRead);}
        unsigned char Config=0; Config |=/*VTF1*/1<<7 | /*VFT0*/ 1<<6 | /*CAPF2*/ 1<<5 | /*CAPF1*/ 1<<4 | /*CAPF0*/ 0<<3 | /*MD2*/ 0<<2 | /*MD1*/ 0<<1 | /*MD0*/ 1;
        writeConfigurationRegister(Config);
        {int configcRead=readConfigurationRegister(); printf("configcRead= 0x%x\n",configcRead);}
        
        // obtenu apres calibration grace au soft windows
        unsigned char CapDacA=0; CapDacA |=/*DACAENA*/1<<7 |  0x3d;
        writeCapDacARegister(CapDacA);
        {int CapDacARead=readCapDacARegister(); printf("CapDacARead= 0x%x\n",CapDacARead);}
        //writeCapDacBRegister(CapDacA);
        //{int CapDacBRead=readCapDacBRegister(); printf("CapDacBRead= 0x%x\n",CapDacBRead);}
        
        
    }
    
    void disableTempRead(){
        int readv=readVtSetupRegister();
        if( ((readv>>7) & 0x1) ==1 ){
            readv^= 1<<7; // flip bit at 7
            writeVtSetupRegister(readv);
        }
    }
    void enableTempRead(){
        int readv=readVtSetupRegister();
        if( ((readv>>7) & 0x1) ==0 ){
            readv^= 1<<7; // flip bit at 7
            writeVtSetupRegister(readv);
        }
    }
    void disableCapaRead(){
        int readv=readCapSetupRegister();
        if( ((readv>>7) & 0x1) ==1 ){
            readv^= 1<<7; // flip bit at 7
            writeCapSetupRegister(readv);
        }
    }
    void enableCapaRead(){
        int readv=readCapSetupRegister();
        if( ((readv>>7) & 0x1) ==0 ){
            readv^= 1<<7; // flip bit at 7
            writeCapSetupRegister(readv);
        }
    }

    // void setCin1(){
    //     int setReg=capReg;
    //     if( ((setReg>>6)&1)==1 ) { // should be 0 for cin1
    //         setReg^= 1<<6; // flip bit at 6
    //         writeCapSetupRegister(setReg);
    //     }
    //     //printf("setcin1  capareg= 0x%x\n",capReg);
    // }
    // void setCin2(){
    //     int setReg=capReg;
    //     if( ((setReg>>6)&1)==0 ) { // should be 1 for cin2
    //         setReg^= 1<<6; // flip bit at 6
    //         writeCapSetupRegister(setReg);
    //     }
    //     //printf("setcin2  capareg= 0x%x\n",capReg);
    // }

    void setCin1() {
        int reg = readCapSetupRegister();   // read the real hardware register
        if (reg < 0) return;                // if I2C failed, don’t change anything
        reg &= ~(1 << 6);                   // CIN2 bit = 0  (select CIN1)
        writeCapSetupRegister((unsigned char)reg);
    }

    void setCin2() {
        int reg = readCapSetupRegister();   // read the real hardware register
        if (reg < 0) return;
        reg |= (1 << 6);                    // CIN2 bit = 1  (select CIN2)
        writeCapSetupRegister((unsigned char)reg);
    }


    void setCapaDac_A( int dac){
        if( dac != (CapDacAReg&0x7f)) writeCapDacARegister( 1<<7 | dac);
    }
    void setCapaDac_B( int dac){
        if( dac != (CapDacBReg&0x7f)) writeCapDacBRegister( 1<<7 | dac);
    }
    
    void setSingleMode(int convSpeedCapa=-1 /* 3 bits de 0x0 a 0x7*/, int convSpeedVT=-1 /* 2 bits de 0x0 a 0x3*/, bool verbose=true){
        if(verbose) printf("Setting single mode\n");
        singleConversion=true;
        ConfigReg=0;
        if(convSpeedCapa>=0x0 && convSpeedCapa<=0x7) ConfigReg |= (convSpeedCapa<<3);
        else ConfigReg |= /*CAPF2*/ 1<<5 | /*CAPF1*/ 1<<4 | /*CAPF0*/ 0<<3; // 92 ms conversion
        if(convSpeedVT>=0x0 && convSpeedVT<=0x3) ConfigReg |= (convSpeedVT<<6);
        else ConfigReg |= /*VTF1*/1<<7 | /*VFT0*/ 0<<6; // 62 ms conversion
        ConfigReg |= /*MD2*/ 0<<2 | /*MD1*/ 1<<1 | /*MD0*/ 0; // single mode
        writeConfigurationRegister(ConfigReg);
    }
    void setContMode(int convSpeedCapa=-1 /* 3 bits de 0x0 a 0x7*/, int convSpeedVT=-1 /* 2 bits de 0x0 a 0x3*/, bool verbose=true){
        if(verbose) printf("Setting continuous mode\n");
        singleConversion=false;
        ConfigReg=0;
        if(convSpeedCapa>=0x0 && convSpeedCapa<=0x7) ConfigReg |= (convSpeedCapa<<3);
        else ConfigReg |= /*CAPF2*/ 1<<5 | /*CAPF1*/ 1<<4 | /*CAPF0*/ 0<<3; // 92 ms conversion
        if(convSpeedVT>=0x0 && convSpeedVT<=0x3) ConfigReg |= (convSpeedVT<<6);
        else ConfigReg |= /*VTF1*/1<<7 | /*VFT0*/ 0<<6; // 62 ms conversion
        ConfigReg |= /*MD2*/ 0<<2 | /*MD1*/ 0<<1 | /*MD0*/ 1; // continuous mode
        writeConfigurationRegister(ConfigReg);
    }
    
    
    
    // do 1 read and exit whatever the status
    // int read_data( bool verbose=false){
    //     status=0;
    //     //		int transferred,rv;
    //     //		unsigned char buf[64];
    //     //		buf[0]=AD7746_READ_DATA;
    //     //		rv=libusb_bulk_transfer(handle,0x1,buf,1,&transferred,64);
    //     //		if(rv) {
    //     //		 printf ( "OUT Transfer failed: %d\n", rv );
    //     //		}
    //     //		rv=libusb_bulk_transfer(handle,0x81,(unsigned char*)buf,20,&transferred,64);
    //     //		if(rv!=0) {
    //     //		 printf ( "IN Transfer failed: %d\n", rv );
    //     //			return rv;
    //     //		}else{
    //     //			if(verbose) printf ( "IN Bulk 1 Transfer OK!! nbytes=%d    val=0x%x\n", transferred, buf[0]);
    //     //		}
    //     //		unsigned int data[64]={0};
    //     //		for(int i=0;i<transferred;i++) data[i]=buf[i]&0xFF;
    //     //		status=data[0];
    //     //		//for(int i=0;i<transferred;i++) printf("0x%x  ",data[i]); printf("\n");
    //     //		if( ((status>>2) & 0x1) ==0 ) ADready4conversion=true;
    //     //		capaDAC = (data[1] << 16) | (data[2] << 8) | data[3];
    //     //		VTDAC =   (data[4] << 16) | (data[5] << 8) | data[6];
    //     //		if(verbose) printf("status=0x%x,  capaDAC=0x%x,  VTDAC=0x%x (%u)\n",status,capaDAC,VTDAC,VTDAC);
    //     //		//printf("status=0x%x,  capaDAC=0x%x,  VTDAC=0x%x (%u)\n",status,capaDAC,VTDAC,VTDAC);
    //     //		if( ((status >> 1) & 0x1) ==0 ) {temperature=VTDAC; temperature= temperature/2048.-4096.;temperatureRead=true;}
    //     //		if( ((status >> 0) & 0x1) ==0 ) {
    //     //			capacite=capaDAC;
    //     //			capacite=(capacite-(double)(0x800000))/(double)(0x800000) *4.096  ; // 0x800000=8388608  il ne faut pas utiliser Cref ici, le gain ne s'applique pas.
    //     //			capacite*=Fextended;
    //     //			capaciteRead=true;
    //     //			if( ((capReg>>6)&1)==0 ) capaciteCin1=capacite;
    //     //			else capaciteCin2=capacite;
    //     //            outRange=false;
    //     //            if( capaDAC==0 || capaDAC==0xFFFFFF) outRange=true;
    //     //		}
    //     //
    //     return status;
    // }
    // double getcapacite(enumChannel icin){
    //     if(icin==cin1) return capaciteCin1;
    //     if(icin==cin2) return capaciteCin2;
    //     return 0.;
    // }
    
    int read_data(bool verbose=false){
    // Read: STATUS (0x00) + CAP(0x01..0x03) + VT(0x04..0x06)
    uint8_t buf[7] = {0};

    Wire.beginTransmission(AD7746_ADDRESS);
    Wire.write((uint8_t)AD7746_RA_STATUS);          // 0x00
    int e = Wire.endTransmission(false);            // repeated start
    if (e != 0) {
        if (verbose) { Serial.print("AD7746 ptr set err="); Serial.println(e); }
        return e;
    }

    int n = Wire.requestFrom((int)AD7746_ADDRESS, 7, (int)true);
    if (n != 7) {
        if (verbose) { Serial.print("AD7746 read n="); Serial.println(n); }
        return 9; // arbitrary nonzero error
    }

   // for (int i=0; i<7; i++) buf[i] = Wire.read();

    for (int i = 0; i < 7; i++) {
        if (Wire.available()) {
            buf[i] = (uint8_t)Wire.read();
        } else {
            // If this ever triggers, your "raw" numbers are not trustworthy.
            // Print ONCE per failure, not every cycle.
            Serial.print("[AD] SHORT READ: Wire.available()=0 at i=");
            Serial.println(i);
            return 10;
        }
    }


    status = buf[0];

    // Datasheet bit meanings are already encoded in your header:
    // RDY bit is bit2, RDYVT bit1, RDYCAP bit0 :contentReference[oaicite:4]{index=4}
    if (((status >> AD7746_RDY_BIT) & 0x1) == 0) ADready4conversion = true;

    capaDAC = ((unsigned int)buf[1] << 16) | ((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
    VTDAC   = ((unsigned int)buf[4] << 16) | ((unsigned int)buf[5] << 8) | (unsigned int)buf[6];

    if (((status >> AD7746_RDYVT_BIT) & 0x1) == 0) {
        // Leave temperature conversion alone if you don't need it
        temperatureRead = true;
    }

    if (((status >> AD7746_RDYCAP_BIT) & 0x1) == 0) {
        // Convert raw 24-bit to +/-4.096 pF “relative” reading (this matches your original commented math)
        capacite = (double)capaDAC;
        capacite = (capacite - 8388608.0) / 8388608.0 * 4.096;
        capacite *= Fextended;
        capaciteRead = true;

        if (((capReg >> 6) & 1) == 0) capaciteCin1 = capacite;
        else                          capaciteCin2 = capacite;

        outRange = (capaDAC == 0 || capaDAC == 0xFFFFFF);
    }

    return 0;
}


    
    // start conversion and exit, reading will be done latter
    int startConversion(bool verbose=false){
        wait4Ready();
        if( singleConversion && !conversionStarted) {
            writeConfigurationRegister(ConfigReg|0x2); // lance une conversion single
            ADready4conversion=false;
            //int r=readConfigurationRegister();
            //printf("0x%x r=%d\n",identifiant,r);
        }else if(!singleConversion){
            ADready4conversion=false;
        }
        conversionStarted=true;
        return 0;
    }
    
    /* get the data (for the current cap channel+ temperature) until status is ok for each or nb of try is 10*/
    int get_data( bool verbose=false){
        temperatureRead=capaciteRead=false;
        //		VTReg=readVtSetupRegister();
        //		capReg=readCapSetupRegister();
        //		if( ((VTReg>>7)&1)==1 ) printf("temp enable");
        //		if( ((capReg>>7)&1)==1 ) printf("capa enable");
        //		printf("\n");
        if( ((VTReg>>7)&1)==0) temperatureRead=true; // temperature/Voltage is not converted
        if( ((capReg>>7)&1)==0) capaciteRead=true; // capacitance is not converted
        if( !conversionStarted) startConversion(verbose);
        int itry=0;
        //printf("%d %d\n",temperatureRead,capaciteRead);
        if(ADready4conversion) printf("**get_data** probleme ADready4conversion=true!\n");
        while( (!temperatureRead || !capaciteRead || !ADready4conversion)  && itry<1000) {
            if(itry>0)   delayMicroseconds(waitInus);
            read_data(verbose);
            //if(temperatureRead) printf("try=%d   status=0x%x,  VTDAC=0x%x (%u) temp=%f\n",itry,status,VTDAC,VTDAC,temperature);
            //if(capaciteRead)    printf("try=%d   status=0x%x,  capaDAC=0x%x (capa=%f pF)\n",itry,status,capaDAC,capacite);
            //  delayMicroseconds(waitInus);
            itry++;
        }
        if(itry>999) {
            printf("AD7746Class::get_data** could not get the conversion!!   itry=%d, waitInus=%d\n",itry,waitInus);
            read_data(true);
        }
        //printf("AD7746Class::get_data**  itry=%d, waitInus=%d\n",itry,waitInus);
        if(temperatureRead && capaciteRead) status=0;
        conversionStarted=false;
        return status;
    }
    
    /* change the current cap channel if needed and get the data*/
    int get_data(enumChannel icin, bool verbose=false){
        if(icin==cin1) return get_dataCin1(verbose);
        else if(icin==cin2) return get_dataCin2(verbose);
        return -1;
    }
    int get_dataCin2( bool verbose=false){
        startConversionCin2(verbose);
        int status=get_data(verbose);
        conversionStartedCin2=false;
        return status;
    }
    int get_dataCin1( bool verbose=false){
        startConversionCin1(verbose);
        int status=get_data(verbose);
        conversionStartedCin1=false;
        return status;
    }
    int get_dataTemperature( bool verbose=false){
        enableTempRead();
        int status=get_data(verbose);
        return status;
    }
    
    
    /* change the current cap channel if needed and start the conversion*/
    int startConversionCin2( bool verbose=false){
        if( isCin2ConversionStarted() ) return 0; // already started
        else if( !ADready4conversion ) wait4Ready(); // conversion is under way but not for cin2
        
        while( ((capReg>>6)&1)!=1 || ((capReg>>7)&1)!=1){ // doit etre 1 pour le cin2 et 1 pour le enable
            setCin2();
        }
        enableCapaRead();
        //setCapaDac_A(calibratedCapaDac_A_cin2);
        int status=startConversion(verbose);
        conversionStartedCin2=true;
        return status;
    }

    int startConversionCin1( bool verbose=false){
        if( isCin1ConversionStarted() ) return 0; // already started
        else if( !ADready4conversion ) wait4Ready(); // conversion is under way but not for cin1

        while( ((capReg>>6)&1)!=0 || ((capReg>>7)&1)!=1){ // must be CIN2=0 and CAPEN=1
            setCin1();
        }
        enableCapaRead();

        // *** STEP 1: do NOT auto-write CAPDAC here ***
        // setCapaDac_A(calibratedCapaDac_A_cin1);

        int status=startConversion(verbose);
        conversionStartedCin1=true;
        return status;
    }

    // int startConversionCin1( bool verbose=false){
    //     if( isCin1ConversionStarted() ) return 0; // already started
    //     else if( !ADready4conversion ) wait4Ready(); // conversion is under way but not for cin1
        
    //     while( ((capReg>>6)&1)!=0 || ((capReg>>7)&1)!=1){ // doit etre 0 pour le cin1 et 1 pour le enable
    //         setCin1();
    //     }
    //     enableCapaRead();
    //     setCapaDac_A(calibratedCapaDac_A_cin1);
    //     //setCapaDac_B(calibratedCapaDac_A_cin1); // empeche la lecture corecte???????????
    //     int status=startConversion(verbose);
    //     conversionStartedCin1=true;
    //     return status;
    // }
    int startConversionTemperature( bool verbose=false){
        enableTempRead();
        int status=startConversion(verbose);
        return status;
    }
    
    

    
    int i2c_read_reg(unsigned char addr, bool verbose=false){
        int ret=0;
        // set the pointer address of the AD7746 that we want to read
        Wire.beginTransmission(AD7746_ADDRESS);
        Wire.write(addr);  // set register for read
        ret=Wire.endTransmission(false); // si true un stop est emis et le AD7746 remer son pointeur d'adresse a 0
        if(ret!=0){
            Serial.print("i2c_read_reg error code= " );Serial.println(ret);
            // https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
        }
        int mr=Wire.requestFrom(AD7746_ADDRESS, 1, true);    // request 1 byte from peripheral device + stop condition
        if (mr != 1) return -1;
        while (Wire.available()) { // peripheral may send less than requested
            ret = Wire.read(); // receive a byte as character
            if (Wire.available()) ret = Wire.read();
        }
        return ret;
    }
    int i2c_write_reg(unsigned char addr, unsigned char data, bool verbose=false){
        int rv=0;
        Wire.beginTransmission(AD7746_ADDRESS);
        Wire.write(addr);  // set register for read
        Wire.write(data);  // set register for read
        rv=Wire.endTransmission(true); // true pour un stop
        if(rv!=0){
            Serial.print("i2c_write_reg error code= " );Serial.println(rv);
            // https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
        }
        return rv;
    }
    
    
    
    int readStatusReg(){
        int reg=i2c_read_reg(AD7746_RA_STATUS,false);
        //printf("statusRegRead= 0x%x\n",reg);
        return reg;
    }
    
    // void reset(){
    //     i2c_write_reg(AD7746_RESET,1,false);
    //     delayMicroseconds(300); // ad7746 is not responding for at most 200 usec after reset
    //     conversionStarted=false;
    // }
    
    void reset(){
    // AD7746 explicit reset: send ONLY the command word 0xBF (no data byte)
    Wire.beginTransmission(AD7746_ADDRESS);
    Wire.write(AD7746_RESET);   // 0xBF
    int rv = Wire.endTransmission(true); // STOP

    if(rv != 0){
        Serial.print("AD7746 reset error code= ");
        Serial.println(rv);
    }

    // Datasheet: no ACK for ~150us (max 200us) after reset
    delayMicroseconds(300);

    conversionStarted = false;
    conversionStartedCin1 = false;
    conversionStartedCin2 = false;
    ADready4conversion = true; // (safe default; prevents wait4Ready deadlocks)
}

    
    const unsigned char writeRegister(const unsigned char ADDR, const unsigned char data){
        int r1=i2c_write_reg( ADDR , data);
        int tryc=0;
        int reg=0;
        //while( tryc<2 && (r1<0 || (reg=i2c_read_reg( ADDR))  != data   )){
        while (tryc < 5 && (r1 != 0 || (reg = i2c_read_reg(ADDR)) != data)) {

            r1=i2c_write_reg( ADDR, data);
            tryc++;
        }
        if( tryc==2 ) printf("**writeRegister** tryc at max!!\n");
        return (unsigned char) data;
    }
    
    void writeCapSetupRegister(const unsigned char data, bool verbose=false) {
        if(verbose) printf("writing AD7746_RA_CAP_SETUP: 0x%x\n",data);
        capReg=writeRegister(AD7746_RA_CAP_SETUP, data);
    }
    
    // int readCapSetupRegister() {
    //     int capReg=i2c_read_reg( AD7746_RA_CAP_SETUP);
    //     while( capReg<0 ) capReg=i2c_read_reg(AD7746_RA_CAP_SETUP);
    //     return capReg;
    // }

    int readCapSetupRegister() {
    int capReg = -1;
    for (int i = 0; i < 5; i++) {
        capReg = i2c_read_reg(AD7746_RA_CAP_SETUP);
        if (capReg >= 0) return capReg;
        delayMicroseconds(200);
    }
    return -1; // caller must handle
}

    
    void writeVtSetupRegister(const unsigned char data, bool verbose=false) {
        if(verbose) printf("writing AD7746_RA_VT_SETUP: 0x%x\n",data);
        VTReg=writeRegister(AD7746_RA_VT_SETUP, data);
    }
    int readVtSetupRegister() {
        int VTReg=i2c_read_reg( AD7746_RA_VT_SETUP);
        return VTReg;
    }
    
    
    void writeExcSetupRegister(const unsigned char data) {
        printf("writing AD7746_RA_EXC_SETUP: 0x%x\n",data);
        writeRegister(AD7746_RA_EXC_SETUP, data);
    }
    int readExcSetupRegister() {
        int ExcReg=i2c_read_reg( AD7746_RA_EXC_SETUP);
        return ExcReg;
    }
    
    
    void writeConfigurationRegister(const unsigned char data, bool verbose=false) {
        if(verbose) printf("writing AD7746_RA_CONFIGURATION: 0x%x\n",data);
        ConfigReg=writeRegister(AD7746_RA_CONFIGURATION, data);
        ConfigReg&= 0xf8; // ne garde que le setup de vitesse (capa et temperature)
        waitInus=1000; // par defaut
        if(singleConversion) {
            waitInus+= (((ConfigReg>>3)&0x7))*1000; // a peu pres 1 ms/dac de conversion pour la capa
            waitInus+= (((ConfigReg>>6)&0x3))*2000; // a peu pres 2 ms/dac de conversion pour la temperature
        }
    }
    int readConfigurationRegister() {
        int configReg=i2c_read_reg( AD7746_RA_CONFIGURATION);
        return configReg;
    }
    
    // data from 0x0=0pF to 0x7F =17pF not factory calibrated
    void writeCapDacRegister(unsigned char addr, const unsigned char data) {
        i2c_write_reg( addr, data);
    }
    // data from 0x0=0pF to 0x7F =17pF non factory calibrated
    void writeCapDacARegister(const unsigned char data, bool verbose=false) {
        if(verbose) printf("writing AD7746_RA_CAP_DAC_A: 0x%x\n",data);
        CapDacAReg=writeRegister(AD7746_RA_CAP_DAC_A, data);
    }
    int readCapDacARegister() {
        int Reg=i2c_read_reg( AD7746_RA_CAP_DAC_A);
        return Reg;
    }
    
    // data from 0x0=0pF to 0x7F =17pF non factory calibrated
    void writeCapDacBRegister(const unsigned char data, bool verbose=false) {
        if(verbose) printf("writing AD7746_RA_CAP_DAC_B: 0x%x\n",data);
        CapDacBReg=writeRegister(AD7746_RA_CAP_DAC_B, data);
    }
    int readCapDacBRegister() {
        int Reg=i2c_read_reg( AD7746_RA_CAP_DAC_B);
        return Reg;
    }
    
    
    
    
    void writeCapOffsetRegister(int reg) {
        int b1= (reg>>8) & 0xFF;
        int b2= reg & 0xFF;
        writeRegister( AD7746_RA_CAP_OFF_H, b1);
        writeRegister( AD7746_RA_CAP_OFF_L, b2);
    }
    double readCapOffsetRegister() {
        int b1=i2c_read_reg( AD7746_RA_CAP_OFF_H);
        int b2=i2c_read_reg( AD7746_RA_CAP_OFF_L);
        capaOffsetDAC=b1<<8|b2;
        capaOffset=(capaOffsetDAC-32768)/32768.;
        return capaOffset;
    }
    double readCapGAINRegister() {
        int b1=i2c_read_reg( AD7746_RA_CAP_GAIN_H);
        int b2=i2c_read_reg( AD7746_RA_CAP_GAIN_L);
        capaGAIN=b1<<8|b2; // factory calibrated
        capaGAIN= ( 65536. + capaGAIN) / 65536.;
        Cref=4.096*capaGAIN;
        Ccapdac=Cref*ratioCrefCapdDacC; // par construction du AD7746
        CcapdacStep=Ccapdac/127./*0x7f*/; // !!! attention non lineaire
        return capaGAIN;
    }
    double readVoltGAINRegister() {
        int b1=i2c_read_reg( AD7746_RA_VOLT_GAIN_H);
        int b2=i2c_read_reg( AD7746_RA_VOLT_GAIN_L);
        VoltGAIN=b1<<8|b2;
        return VoltGAIN;
    }
    double readIntTempGAINRegister() {
        int b1=i2c_read_reg( 0x17);
        int b2=i2c_read_reg( 0x18);
        //printf("b1=%d(0x%x),  b2=%d\n",b1,b1,b2);
        IntTempGAIN=b1<<8|b2;
        //IntTempGAIN= ( (double)(2<<16)+ capaGAIN) / (double)(2<<16);
        return IntTempGAIN;
    }
    double readIntTempOffRegister() {
        int b1=i2c_read_reg( 0x13);
        int b2=i2c_read_reg( 0x14);
        //printf("b1=%d(0x%x),  b2=%d\n",b1,b1,b2);
        IntTempOffset=b1<<8|b2;
        //IntTempOffset= ( (double)(2<<16)+ capaGAIN) / (double)(2<<16);
        return IntTempOffset;
    }
    
    
    int readChipRevisonRegister() {
        chipRevision=0;
        chipRevision=i2c_read_reg( 0x1B);
        return chipRevision;
    }
    
    
    
    // scan des proprietes copie de test_device de libusb-1.0.24/examples/xusb.c
    //	 int openDevice(bool printAll=true){
    
    
    
};



#endif