/*
 * config.c: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 1.54 2001/08/11 15:34:42 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include "dvbapi.h"
#include "i18n.h"
#include "interface.h"

// -- cKeys ------------------------------------------------------------------

tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,            "Up",            0 },
                    { kDown,          "Down",          0 },
                    { kMenu,          "Menu",          0 },
                    { kOk,            "Ok",            0 },
                    { kBack,          "Back",          0 },
                    { kLeft,          "Left",          0 },
                    { kRight,         "Right",         0 },
                    { kRed,           "Red",           0 },
                    { kGreen,         "Green",         0 },
                    { kYellow,        "Yellow",        0 },
                    { kBlue,          "Blue",          0 },
                    { k0,             "0",             0 },
                    { k1,             "1",             0 },
                    { k2,             "2",             0 },
                    { k3,             "3",             0 },
                    { k4,             "4",             0 },
                    { k5,             "5",             0 },
                    { k6,             "6",             0 },
                    { k7,             "7",             0 },
                    { k8,             "8",             0 },
                    { k9,             "9",             0 },
                    { kNone,          "",              0 },
                  };

cKeys::cKeys(void)
{
  fileName = NULL;
  code = 0;
  address = 0;
  keys = keyTable;
}

void cKeys::Clear(void)
{
  for (tKey *k = keys; k->type != kNone; k++)
      k->code = 0;
}

void cKeys::SetDummyValues(void)
{
  for (tKey *k = keys; k->type != kNone; k++)
      k->code = k->type + 1; // '+1' to avoid 0
}

bool cKeys::Load(const char *FileName)
{
  isyslog(LOG_INFO, "loading %s", FileName);
  bool result = false;
  if (FileName)
     fileName = strdup(FileName);
  if (fileName) {
     FILE *f = fopen(fileName, "r");
     if (f) {
        int line = 0;
        char buffer[MaxBuffer];
        result = true;
        while (fgets(buffer, sizeof(buffer), f) > 0) {
              line++;
              if (!isempty(buffer)) {
                 char *Name = buffer;
                 char *p = strpbrk(Name, " \t");
                 if (p) {
                    *p = 0; // terminates 'Name'
                    while (*++p && isspace(*p))
                          ;
                    if (*p) {
                       if (strcasecmp(Name, "Code") == 0)
                          code = *p;
                       else if (strcasecmp(Name, "Address") == 0)
                          address = strtol(p, NULL, 16);
                       else {
                          for (tKey *k = keys; k->type != kNone; k++) {
                              if (strcasecmp(Name, k->name) == 0) {
                                 k->code = strtol(p, NULL, 16);
                                 Name = NULL; // to indicate that we found it
                                 break;
                                 }
                              }
                           if (Name) {
                              esyslog(LOG_ERR, "unknown key in %s, line %d\n", fileName, line);
                              result = false;
                              break;
                              }
                           }
                       }
                    continue;
                    }
                 esyslog(LOG_ERR, "error in %s, line %d\n", fileName, line);
                 result = false;
                 break;
                 }
              }
        fclose(f);
        }
     else
        esyslog(LOG_ERR, "can't open '%s'\n", fileName);
     }
  else
     esyslog(LOG_ERR, "no key configuration file name supplied!\n");
  return result;
}

bool cKeys::Save(void)
{
  bool result = true;
  cSafeFile f(fileName);
  if (f.Open()) {
     if (fprintf(f, "Code\t%c\nAddress\t%04X\n", code, address) > 0) {
        for (tKey *k = keys; k->type != kNone; k++) {
            if (fprintf(f, "%s\t%08X\n", k->name, k->code) <= 0) {
               result = false;
               break;
               }
            }
         }
     else
        result = false;
     f.Close();
     }
  else
     result = false;
  return result;
}

eKeys cKeys::Get(unsigned int Code)
{
  if (Code != 0) {
     tKey *k;
     for (k = keys; k->type != kNone; k++) {
         if (k->code == Code)
            break;
         }
     return k->type;
     }
  return kNone;
}

eKeys cKeys::Translate(const char *Command)
{
  if (Command) {
     const tKey *k = keys;
     while ((k->type != kNone) && strcasecmp(k->name, Command) != 0)
           k++;
     return k->type;
     }
  return kNone;
}

unsigned int cKeys::Encode(const char *Command)
{
  eKeys k = Translate(Command);
  if (k != kNone)
     return keys[k].code;
  return 0;
}

void cKeys::Set(eKeys Key, unsigned int Code)
{
  for (tKey *k = keys; k->type != kNone; k++) {
      if (k->type == Key) {
         k->code = Code;
         break;
         }
      }
}

// -- cChannel ---------------------------------------------------------------

char *cChannel::buffer = NULL;

cChannel::cChannel(void)
{
  *name = 0;
}

cChannel::cChannel(const cChannel *Channel)
{
  strcpy(name,   Channel ? Channel->name         : "Pro7");
  frequency    = Channel ? Channel->frequency    : 12480;
  polarization = Channel ? Channel->polarization : 'v';
  diseqc       = Channel ? Channel->diseqc       : 0;
  srate        = Channel ? Channel->srate        : 27500;
  vpid         = Channel ? Channel->vpid         : 255;
  apid1        = Channel ? Channel->apid1        : 256;
  apid2        = Channel ? Channel->apid2        : 0;
  dpid1        = Channel ? Channel->dpid1        : 257;
  dpid2        = Channel ? Channel->dpid2        : 0;
  tpid         = Channel ? Channel->tpid         : 32;
  ca           = Channel ? Channel->ca           : 0;
  pnr          = Channel ? Channel->pnr          : 0;
  groupSep     = Channel ? Channel->groupSep     : false;
}

const char *cChannel::ToText(cChannel *Channel)
{
  char buf[MaxChannelName * 2];
  char *s = Channel->name;
  if (strchr(s, ':')) {
     s = strcpy(buf, s);
     strreplace(s, ':', '|');
     }
  delete buffer;
  if (Channel->groupSep)
     asprintf(&buffer, ":%s\n", s);
  else {
     char apidbuf[32];
     char *q = apidbuf;
     q += snprintf(q, sizeof(apidbuf), "%d", Channel->apid1);
     if (Channel->apid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->apid2);
     if (Channel->dpid1 || Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ";%d", Channel->dpid1);
     if (Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->dpid2);
     *q = 0;
     asprintf(&buffer, "%s:%d:%c:%d:%d:%d:%s:%d:%d:%d\n", s, Channel->frequency, Channel->polarization, Channel->diseqc, Channel->srate, Channel->vpid, apidbuf, Channel->tpid, Channel->ca, Channel->pnr);
     }
  return buffer;
}

const char *cChannel::ToText(void)
{
  return ToText(this);
}

bool cChannel::Parse(const char *s)
{
  char *buffer = NULL;
  if (*s == ':') {
     if (*++s) {
        strn0cpy(name, s, MaxChannelName);
        name[strlen(name) - 1] = 0; // strip the '\n'
        groupSep = true;
        }
     else
        return false;
     }
  else {
     groupSep = false;
     char *apidbuf = NULL;
     int fields = sscanf(s, "%a[^:]:%d:%c:%d:%d:%d:%a[^:]:%d:%d:%d", &buffer, &frequency, &polarization, &diseqc, &srate, &vpid, &apidbuf, &tpid, &ca, &pnr);
     apid1 = apid2 = 0;
     dpid1 = dpid2 = 0;
     if (apidbuf) {
        char *p = strchr(apidbuf, ';');
        if (p)
           *p++ = 0;
        sscanf(apidbuf, "%d,%d", &apid1, &apid2);
        if (p)
           sscanf(p, "%d,%d", &dpid1, &dpid2);
        delete apidbuf;
        }
     else
        return false;
     if (fields >= 9) {
        if (fields == 9) {
           // allow reading of old format
           pnr = ca;
           ca = tpid;
           tpid = 0;
           }
        strn0cpy(name, buffer, MaxChannelName);
        delete buffer;
        }
     else
        return false;
     }
  strreplace(name, '|', ':');
  return true;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

bool cChannel::Switch(cDvbApi *DvbApi, bool Log)
{
  if (!DvbApi)
     DvbApi = cDvbApi::PrimaryDvbApi;
  if (!DvbApi->Recording() && !groupSep) {
     if (Log) {
        isyslog(LOG_INFO, "switching to channel %d", number);
        }
     for (int i = 3; i--;) {
         if (DvbApi->SetChannel(number, frequency, polarization, diseqc, srate, vpid, apid1, apid2, dpid1, dpid2, tpid, ca, pnr))
            return true;
         esyslog(LOG_ERR, "retrying");
         }
     return false;
     }
  if (DvbApi->Recording())
     Interface->Info(tr("Channel locked (recording)!"));
  return false;
}

// -- cTimer -----------------------------------------------------------------

char *cTimer::buffer = NULL;

cTimer::cTimer(bool Instant)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = Instant;
  cChannel *ch = Channels.GetByNumber(cDvbApi::CurrentChannel());
  channel = ch ? ch->number : 0;
  time_t t = time(NULL);
  struct tm *now = localtime(&t);
  day = now->tm_mday;
  start = now->tm_hour * 100 + now->tm_min;
  stop = start + 200; // "instant recording" records 2 hours by default
  if (stop >= 2400)
     stop -= 2400;
//TODO VPS???
  priority = Setup.DefaultPriority;
  lifetime = Setup.DefaultLifetime;
  *file = 0;
  summary = NULL;
  if (Instant && ch)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", ch->name);
}

cTimer::cTimer(const cEventInfo *EventInfo)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = true;
  cChannel *ch = Channels.GetByServiceID(EventInfo->GetServiceID());
  channel = ch ? ch->number : 0;
  time_t tstart = EventInfo->GetTime();
  time_t tstop = tstart + EventInfo->GetDuration() + Setup.MarginStop * 60;
  tstart -= Setup.MarginStart * 60;
  struct tm *time = localtime(&tstart);
  day = time->tm_mday;
  start = time->tm_hour * 100 + time->tm_min;
  time = localtime(&tstop);
  stop = time->tm_hour * 100 + time->tm_min;
  if (stop >= 2400)
     stop -= 2400;
  priority = Setup.DefaultPriority;
  lifetime = Setup.DefaultLifetime;
  *file = 0;
  const char *Title = EventInfo->GetTitle();
  if (!isempty(Title))
     strn0cpy(file, EventInfo->GetTitle(), sizeof(file));
  summary = NULL;
  const char *Subtitle = EventInfo->GetSubtitle();
  if (isempty(Subtitle))
     Subtitle = "";
  const char *Summary = EventInfo->GetExtendedDescription();
  if (isempty(Summary))
     Summary = "";
  if (*Subtitle || *Summary) {
     asprintf(&summary, "%s%s%s", Subtitle, (*Subtitle && *Summary) ? "\n\n" : "", Summary);
     char *p = summary;
     while (*p) {
           if (*p == '\n')
              *p = '|';
           p++;
           }
     }
}

cTimer::~cTimer()
{
  delete summary;
}

cTimer& cTimer::operator= (const cTimer &Timer)
{
  memcpy(this, &Timer, sizeof(*this));
  if (summary)
     summary = strdup(summary);
  return *this;
}

const char *cTimer::ToText(cTimer *Timer)
{
  delete buffer;
  strreplace(Timer->file, ':', '|');
  strreplace(Timer->summary, '\n', '|');
  asprintf(&buffer, "%d:%d:%s:%04d:%04d:%d:%d:%s:%s\n", Timer->active, Timer->channel, PrintDay(Timer->day), Timer->start, Timer->stop, Timer->priority, Timer->lifetime, Timer->file, Timer->summary ? Timer->summary : "");
  strreplace(Timer->summary, '|', '\n');
  strreplace(Timer->file, '|', ':');
  return buffer;
}

const char *cTimer::ToText(void)
{
  return ToText(this);
}

int cTimer::TimeToInt(int t)
{
  return (t / 100 * 60 + t % 100) * 60;
}

time_t cTimer::Day(time_t t)
{
  struct tm d = *localtime(&t);
  d.tm_hour = d.tm_min = d.tm_sec = 0;
  return mktime(&d);
}

int cTimer::ParseDay(const char *s)
{
  char *tail;
  int d = strtol(s, &tail, 10);
  if (tail && *tail) {
     d = 0;
     if (tail == s) {
        if (strlen(s) == 7) {
           for (const char *p = s + 6; p >= s; p--) {
                 d <<= 1;
                 d |= (*p != '-');
                 }
           d |= 0x80000000;
           }
        }
     }
  else if (d < 1 || d > 31)
     d = 0;
  return d;
}

const char *cTimer::PrintDay(int d)
{
  static char buffer[8];
  if ((d & 0x80000000) != 0) {
     char *b = buffer;
     const char *w = tr("MTWTFSS");
     *b = 0;
     while (*w) {
           *b++ = (d & 1) ? *w : '-';
           d >>= 1;
           w++;
           }
     }
  else
     sprintf(buffer, "%d", d);
  return buffer;
}

bool cTimer::Parse(const char *s)
{
  char *buffer1 = NULL;
  char *buffer2 = NULL;
  delete summary;
  summary = NULL;
  //XXX Apparently sscanf() doesn't work correctly if the last %a argument
  //XXX results in an empty string (this first occured when the EIT gathering
  //XXX was put into a separate thread - don't know why this happens...
  //XXX As a cure we copy the original string and add a blank.
  //XXX If anybody can shed some light on why sscanf() failes here, I'd love
  //XXX to hear about that!
  char *s2 = NULL;
  int l2 = strlen(s);
  while (l2 > 0 && isspace(s[l2 - 1]))
        l2--;
  if (s[l2 - 1] == ':') {
     s2 = (char *)malloc(l2 + 3);
     strcat(strn0cpy(s2, s, l2 + 1), " \n");
     s = s2;
     }
  if (8 <= sscanf(s, "%d:%d:%a[^:]:%d:%d:%d:%d:%a[^:\n]:%a[^\n]", &active, &channel, &buffer1, &start, &stop, &priority, &lifetime, &buffer2, &summary)) {
     if (summary && !*skipspace(summary)) {
        delete summary;
        summary = NULL;
        }
     //TODO add more plausibility checks
     day = ParseDay(buffer1);
     strn0cpy(file, buffer2, MaxFileName);
     strreplace(file, '|', ':');
     strreplace(summary, '|', '\n');
     delete buffer1;
     delete buffer2;
     delete s2;
     return day != 0;
     }
  delete s2;
  return false;
}

bool cTimer::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

bool cTimer::IsSingleEvent(void)
{
  return (day & 0x80000000) == 0;
}

bool cTimer::Matches(time_t t)
{
  if (active) {
     if (t == 0)
        t = time(NULL);
     struct tm now = *localtime(&t);
     int weekday = now.tm_wday == 0 ? 6 : now.tm_wday - 1; // we start with monday==0!
     int begin = TimeToInt(start);
     int end   = TimeToInt(stop);
     bool twoDays = (end < begin);

     bool todayMatches = false, yesterdayMatches = false;
     if ((day & 0x80000000) != 0) {
        if ((day & (1 << weekday)) != 0)
           todayMatches = true;
        else if (twoDays) {
           int yesterday = weekday == 0 ? 6 : weekday - 1;
           if ((day & (1 << yesterday)) != 0)
              yesterdayMatches = true;
           }
        }
     else if (day == now.tm_mday)
        todayMatches = true;
     else if (twoDays) {
        time_t ty = t - SECSINDAY;
        if (day == localtime(&ty)->tm_mday)
           yesterdayMatches = true;
        }
     if (todayMatches || (twoDays && yesterdayMatches)) {
        startTime = Day(t - (yesterdayMatches ? SECSINDAY : 0)) + begin;
        stopTime  = startTime + (twoDays ? SECSINDAY - begin + end : end - begin);
        }
     else
        startTime = stopTime = 0;
     return startTime <= t && t <= stopTime;
     }
  return false;
}

time_t cTimer::StartTime(void)
{
  if (!startTime)
     Matches();
  return startTime;
}

time_t cTimer::StopTime(void)
{
  if (!stopTime)
     Matches();
  return stopTime;
}

void cTimer::SetRecording(bool Recording)
{
  recording = Recording;
  isyslog(LOG_INFO, "timer %d %s", Index() + 1, recording ? "start" : "stop");
}

void cTimer::SetPending(bool Pending)
{
  pending = Pending;
}

cTimer *cTimer::GetMatch(void)
{
  time_t t = time(NULL); // all timers must be checked against the exact same time to correctly handle Priority!
  cTimer *t0 = NULL;
  cTimer *ti = (cTimer *)Timers.First();
  while (ti) {
        if (!ti->recording && ti->Matches(t)) {
           if (!t0 || ti->priority > t0->priority)
              t0 = ti;
           }
        ti = (cTimer *)ti->Next();
        }
  return t0;
}

// --- cCommand -------------------------------------------------------------

char *cCommand::result = NULL;

cCommand::cCommand(void)
{
  title = command = NULL;
}

cCommand::~cCommand()
{
  delete title;
  delete command;
}

bool cCommand::Parse(const char *s)
{
  const char *p = strchr(s, ':');
  if (p) {
     int l = p - s;
     if (l > 0) {
        title = new char[l + 1];
        strn0cpy(title, s, l + 1);
        if (!isempty(title)) {
           command = stripspace(strdup(skipspace(p + 1)));
           return !isempty(command);
           }
        }
     }
  return false;
}

const char *cCommand::Execute(void)
{
  dsyslog(LOG_INFO, "executing command '%s'", command);
  delete result;
  result = NULL;
  FILE *p = popen(command, "r");
  if (p) {
     int l = 0;
     int c;
     while ((c = fgetc(p)) != EOF) {
           if (l % 20 == 0)
              result = (char *)realloc(result, l + 21);
           result[l++] = c;
           }
     if (result)
        result[l] = 0;
     pclose(p);
     }
  else
     esyslog(LOG_ERR, "ERROR: can't open pipe for command '%s'", command);
  return result;
}

// -- cKeys ------------------------------------------------------------------

cKeys Keys;

// -- cCommands --------------------------------------------------------------

cCommands Commands;

// -- cChannels --------------------------------------------------------------

int CurrentGroup = -1;

cChannels Channels;

bool cChannels::Load(const char *FileName)
{
  if (cConfig<cChannel>::Load(FileName)) {
     ReNumber();
     return true;
     }
  return false;
}

int cChannels::GetNextGroup(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && !channel->groupSep)
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

int cChannels::GetPrevGroup(int Idx)
{
  cChannel *channel = Get(--Idx);
  while (channel && !channel->groupSep)
        channel = Get(--Idx);
  return channel ? Idx : -1;
}

int cChannels::GetNextNormal(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && channel->groupSep)
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

void cChannels::ReNumber( void )
{
  int Number = 0;
  cChannel *ch = (cChannel *)First();
  while (ch) {
        if (!ch->groupSep)
           ch->number = ++Number;
        ch = (cChannel *)ch->Next();
        }
  maxNumber = Number;
}

cChannel *cChannels::GetByNumber(int Number)
{
  cChannel *channel = (cChannel *)First();
  while (channel) {
        if (channel->number == Number)
           return channel;
        channel = (cChannel *)channel->Next();
        }
  return NULL;
}

cChannel *cChannels::GetByServiceID(unsigned short ServiceId)
{
  cChannel *channel = (cChannel *)First();
  while (channel) {
        if (channel->pnr == ServiceId)
           return channel;
        channel = (cChannel *)channel->Next();
        }
  return NULL;
}

bool cChannels::SwitchTo(int Number, cDvbApi *DvbApi)
{
  cChannel *channel = GetByNumber(Number);
  return channel && channel->Switch(DvbApi);
}

const char *cChannels::GetChannelNameByNumber(int Number)
{
  cChannel *channel = GetByNumber(Number);
  return channel ? channel->name : NULL;
}

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

cTimer *cTimers::GetTimer(cTimer *Timer)
{
  cTimer *ti = (cTimer *)First();
  while (ti) {
        if (ti->channel == Timer->channel && ti->day == Timer->day && ti->start == Timer->start && ti->stop == Timer->stop)
           return ti;
        ti = (cTimer *)ti->Next();
        }
  return NULL;
}

// -- cSetup -----------------------------------------------------------------

cSetup Setup;

char *cSetup::fileName = NULL;

cSetup::cSetup(void)
{
  OSDLanguage = 0;
  PrimaryDVB = 1;
  ShowInfoOnChSwitch = 1;
  MenuScrollPage = 1;
  MarkInstantRecord = 1;
  LnbSLOF    = 11700;
  LnbFrequLo =  9750;
  LnbFrequHi = 10600;
  DiSEqC = 0;
  SetSystemTime = 0;
  MarginStart = 2;
  MarginStop = 10;
  EPGScanTimeout = 5;
  SVDRPTimeout = 300;
  PrimaryLimit = 0;
  DefaultPriority = 50;
  DefaultLifetime = 50;
  VideoFormat = VIDEO_FORMAT_4_3;
  ChannelInfoPos = 0;
  OSDwidth = 52;
  OSDheight = 18;
  CurrentChannel = -1;
}

bool cSetup::Parse(char *s)
{
  const char *Delimiters = " \t\n=";
  char *Name  = strtok(s, Delimiters);
  char *Value = strtok(NULL, Delimiters);
  if (Name && Value) {
     if      (!strcasecmp(Name, "OSDLanguage"))         OSDLanguage        = atoi(Value);
     else if (!strcasecmp(Name, "PrimaryDVB"))          PrimaryDVB         = atoi(Value);
     else if (!strcasecmp(Name, "ShowInfoOnChSwitch"))  ShowInfoOnChSwitch = atoi(Value);
     else if (!strcasecmp(Name, "MenuScrollPage"))      MenuScrollPage     = atoi(Value);
     else if (!strcasecmp(Name, "MarkInstantRecord"))   MarkInstantRecord  = atoi(Value);
     else if (!strcasecmp(Name, "LnbSLOF"))             LnbSLOF            = atoi(Value);
     else if (!strcasecmp(Name, "LnbFrequLo"))          LnbFrequLo         = atoi(Value);
     else if (!strcasecmp(Name, "LnbFrequHi"))          LnbFrequHi         = atoi(Value);
     else if (!strcasecmp(Name, "DiSEqC"))              DiSEqC             = atoi(Value);
     else if (!strcasecmp(Name, "SetSystemTime"))       SetSystemTime      = atoi(Value);
     else if (!strcasecmp(Name, "MarginStart"))         MarginStart        = atoi(Value);
     else if (!strcasecmp(Name, "MarginStop"))          MarginStop         = atoi(Value);
     else if (!strcasecmp(Name, "EPGScanTimeout"))      EPGScanTimeout     = atoi(Value);
     else if (!strcasecmp(Name, "SVDRPTimeout"))        SVDRPTimeout       = atoi(Value);
     else if (!strcasecmp(Name, "PrimaryLimit"))        PrimaryLimit       = atoi(Value);
     else if (!strcasecmp(Name, "DefaultPriority"))     DefaultPriority    = atoi(Value);
     else if (!strcasecmp(Name, "DefaultLifetime"))     DefaultLifetime    = atoi(Value);
     else if (!strcasecmp(Name, "VideoFormat"))         VideoFormat        = atoi(Value);
     else if (!strcasecmp(Name, "ChannelInfoPos"))      ChannelInfoPos     = atoi(Value);
     else if (!strcasecmp(Name, "OSDwidth"))            OSDwidth           = atoi(Value);
     else if (!strcasecmp(Name, "OSDheight"))           OSDheight          = atoi(Value);
     else if (!strcasecmp(Name, "CurrentChannel"))      CurrentChannel     = atoi(Value);
     else
        return false;
     return true;
     }
  return false;
}

bool cSetup::Load(const char *FileName)
{
  isyslog(LOG_INFO, "loading %s", FileName);
  delete fileName;
  fileName = strdup(FileName);
  FILE *f = fopen(fileName, "r");
  if (f) {
     int line = 0;
     char buffer[MaxBuffer];
     bool result = true;
     while (fgets(buffer, sizeof(buffer), f) > 0) {
           line++;
           if (!isempty(buffer)) {
              if (*buffer != '#' && !Parse(buffer)) {
                 esyslog(LOG_ERR, "error in %s, line %d\n", fileName, line);
                 result = false;
                 break;
                 }
              }
           }
     fclose(f);
     return result;
     }
  else
     LOG_ERROR_STR(FileName);
  return false;
}

bool cSetup::Save(const char *FileName)
{
  if (!FileName)
     FileName = fileName;
  if (FileName) {
     cSafeFile f(FileName);
     if (f.Open()) {
        fprintf(f, "# VDR Setup\n");
        fprintf(f, "OSDLanguage        = %d\n", OSDLanguage);
        fprintf(f, "PrimaryDVB         = %d\n", PrimaryDVB);
        fprintf(f, "ShowInfoOnChSwitch = %d\n", ShowInfoOnChSwitch);
        fprintf(f, "MenuScrollPage     = %d\n", MenuScrollPage);
        fprintf(f, "MarkInstantRecord  = %d\n", MarkInstantRecord);
        fprintf(f, "LnbSLOF            = %d\n", LnbSLOF);
        fprintf(f, "LnbFrequLo         = %d\n", LnbFrequLo);
        fprintf(f, "LnbFrequHi         = %d\n", LnbFrequHi);
        fprintf(f, "DiSEqC             = %d\n", DiSEqC);
        fprintf(f, "SetSystemTime      = %d\n", SetSystemTime);
        fprintf(f, "MarginStart        = %d\n", MarginStart);
        fprintf(f, "MarginStop         = %d\n", MarginStop);
        fprintf(f, "EPGScanTimeout     = %d\n", EPGScanTimeout);
        fprintf(f, "SVDRPTimeout       = %d\n", SVDRPTimeout);
        fprintf(f, "PrimaryLimit       = %d\n", PrimaryLimit);
        fprintf(f, "DefaultPriority    = %d\n", DefaultPriority);
        fprintf(f, "DefaultLifetime    = %d\n", DefaultLifetime);
        fprintf(f, "VideoFormat        = %d\n", VideoFormat);
        fprintf(f, "ChannelInfoPos     = %d\n", ChannelInfoPos);
        fprintf(f, "OSDwidth           = %d\n", OSDwidth);
        fprintf(f, "OSDheight          = %d\n", OSDheight);
        fprintf(f, "CurrentChannel     = %d\n", CurrentChannel);
        f.Close();
        isyslog(LOG_INFO, "saved setup to %s", FileName);
        return true;
        }
     else
        LOG_ERROR_STR(FileName);
     }
  else
     esyslog(LOG_ERR, "attempt to save setup without file name");
  return false;
}

