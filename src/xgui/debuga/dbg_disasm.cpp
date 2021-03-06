#include "dbg_disasm.h"
#include "dbg_dump.h"

#include <QDebug>
#include <QFont>
#include <QPainter>

extern unsigned short disasmAdr;
extern int blockStart;
extern int blockEnd;

extern QColor colPC;
extern QColor colBRK;
extern QColor colSEL;

QString findLabel(int, int, int);

static int mode = XVIEW_CPU;
static int page = 0;

extern unsigned short adr_of_reg(CPU* cpu, bool* flag, QString nam);

// MODEL

xDisasmModel::xDisasmModel(QObject* p):QAbstractTableModel(p) {
	cptr = NULL;
}

int xDisasmModel::columnCount(const QModelIndex& idx) const {
	return 4;
}

int xDisasmModel::rowCount(const QModelIndex& idx) const {
	return 25;
}

QVariant xDisasmModel::data(const QModelIndex& idx, int role) const {
	QVariant res;
	if (!cptr) return res;
	if (!idx.isValid()) return res;
	int row = idx.row();
	int col = idx.column();
	if (row < 0) return res;
	if (col < 0) return res;
	if (row >= rowCount()) return res;
	if (col >= columnCount()) return res;
	if (row >= dasm.size()) return res;
	QFont font;
	QPixmap pxm;
	QPixmap icn;
	QPainter pnt;
	switch(role) {
		case Qt::EditRole:
			switch(col) {
				case 0: res = dasm[row].aname; break;
				case 1: res = dasm[row].bytes; break;
				case 2: res = dasm[row].command; break;
			}
			break;
		case Qt::FontRole:
			if ((col == 0) && dasm[row].islab) {
				font = QFont();
				font.setBold(true);
				res = font;
			}
			break;
		case Qt::ForegroundRole:
			if (dasm[row].isbrk) {
				res = QColor(Qt::red);
			} else if ((col == 0) && !dasm[row].islab && conf.dbg.hideadr) {
				res = QColor(Qt::gray);
			}
			break;
		case Qt::BackgroundColorRole:
			if (dasm[row].ispc && !dasm[row].islab) {
				res = colPC;			// pc
			} else if (dasm[row].issel) {
				res = colSEL;			// selected
			}
			break;
		case Qt::DecorationRole:
			if ((col == 3) && !dasm[row].icon.isEmpty()) {
				pxm = QPixmap(45,15);
				pxm.fill(Qt::transparent);
				icn.load(dasm[row].icon);
				pnt.begin(&pxm);
				pnt.drawPixmap(pxm.width() - icn.width(),0,icn.scaled(16,16));
				pnt.end();
				res = pxm;
			}
			break;
		case Qt::TextAlignmentRole:
			if (col == 3) res = Qt::AlignLeft;
			break;
		case Qt::DisplayRole:
			switch(col) {
				case 0: res = dasm[row].aname; break;
				case 1: res = dasm[row].bytes; break;
				case 2: res = dasm[row].command; break;
				case 3: res = dasm[row].info; break;
			}
			break;
		case Qt::UserRole:
			switch (col) {
				case 0: res = dasm[row].adr; break;
				case 2: res = dasm[row].oadr; break;
			}

			break;
	}
	return res;
}

unsigned char dasmrd(unsigned short adr, void* ptr) {
	Computer* comp = (Computer*)ptr;
	unsigned char res = 0xff;
	int fadr;
	MemPage* pg;
	switch (mode) {
		case XVIEW_CPU:
			pg = &comp->mem->map[adr >> 8];
			fadr = (pg->num << 8) | (adr & 0xff);
			switch (pg->type) {
				case MEM_ROM: res = comp->mem->romData[fadr & comp->mem->romMask]; break;
				case MEM_RAM: res = comp->mem->ramData[fadr & comp->mem->ramMask]; break;
				case MEM_SLOT:
					if (!comp->slot) break;
					if (!comp->slot->data) break;
					res = comp->slot->data[fadr & comp->slot->memMask];
					break;
			}
//			res = memRd(comp->mem, adr);
			break;
		case XVIEW_RAM:
			res = comp->mem->ramData[((adr & 0x3fff) | (page << 14)) & comp->mem->ramMask];
			break;
		case XVIEW_ROM:
			res = comp->mem->romData[((adr & 0x3fff) | (page << 14)) & comp->mem->romMask];
			break;
	}
	return res;
}

void dasmwr(Computer* comp, unsigned short adr, unsigned char bt) {
	int fadr;
	MemPage* pg;
	switch(mode) {
		case XVIEW_CPU:
			pg = &comp->mem->map[adr >> 8];
			fadr = (pg->num << 8) | (adr & 0xff);
			switch (pg->type) {
				// no writing to rom or slot
				case MEM_RAM: comp->mem->ramData[fadr & comp->mem->ramMask] = bt; break;
			}
//			memWr(comp->mem, adr, bt);
			break;
		case XVIEW_RAM:
			comp->mem->ramData[((adr & 0x3fff) | (page << 14)) & comp->mem->ramMask] = bt;
			break;
		case XVIEW_ROM:
			// comp->mem->romData[((adr & 0x3fff) | (page << 14)) & comp->mem->romMask] = bt;
			break;
	}
}

void placeLabel(Computer* comp, dasmData& drow) {
	int shift = 0;
	int work = 1;
	QString lab;
	QString num;
	xMnem mn;
	while (work && (shift < 8)) {
		lab = findLabel(drow.oadr - shift, -1, -1);
		if (lab.isEmpty()) {
			if (drow.oflag & OF_MEMADR) {
				shift++;
			} else {
				work = 0;
			}
		} else {
			mn.len = 8;					// default seek range
			if ((drow.flag & 0xf0) == DBG_VIEW_CODE) {	// for code - size of opcode only
				mn = cpuDisasm(comp->cpu, (drow.oadr - shift) & 0xffff, NULL, dasmrd, comp);
			}
			if (shift < mn.len) {
				num = gethexword(drow.oadr).prepend("#").toUpper();
				if (shift == 0) {
					drow.command.replace(num, QString("%0").arg(lab));
				} else {
					drow.command.replace(num, QString("%0 + %1").arg(lab).arg(shift));
				}
			}
			work = 0;
		}
	}
}

int dasmByte(Computer* comp, unsigned short adr, dasmData& drow) {
	drow.command = QString("DB #%0").arg(gethexbyte(dasmrd(adr, comp)));
	return 1;
}

int dasmWord(Computer* comp, unsigned short adr, dasmData& drow) {
	int word = dasmrd(adr, comp);
	word |= (dasmrd(adr + 1, comp) << 8);
	drow.command = QString("DW #%0").arg(gethexword(word));
	return 2;
}

int dasmAddr(Computer* comp, unsigned short adr, dasmData& drow) {
	int word = dasmrd(adr, comp);
	word |= (dasmrd(adr + 1, comp) << 8);
	QString lab = findLabel(word & 0xffff, -1, -1);
	if (lab.isEmpty()) {
		lab = gethexword(word).prepend("#");
	}
	drow.command = QString("DW %0").arg(lab);
	placeLabel(comp, drow);
	return 2;
}

int dasmText(Computer* comp, unsigned short adr, dasmData& drow) {
	int clen = 0;
	drow.command = QString("DB \"");
	unsigned char fl = getBrk(comp, adr);
	unsigned char bt = dasmrd(adr, comp);
	while (((fl & 0xf0) == DBG_VIEW_TEXT) && (bt > 31) && (bt < 128) && (clen < 250)) {
		drow.command.append(QChar(bt));
		clen++;
		bt = dasmrd((adr + clen) & 0xffff, comp);
		fl = getBrk(comp, (adr + clen) & 0xffff);
	}
	if (clen == 0) {
		drow.flag = (getBrk(comp, adr) & 0x0f) | DBG_VIEW_BYTE;
		setBrk(comp, adr, drow.flag);
		clen = 1;
		drow.command = QString("DB #%0").arg(dasmrd(adr, comp));
	} else {
		drow.command.append("\"");
	}
	return clen;
}

int dasmCode(Computer* comp, unsigned short adr, dasmData& drow) {
	char buf[1024];
	xMnem mnm = cpuDisasm(comp->cpu, adr, buf, dasmrd, comp);
	drow.command = QString(buf).toUpper();
	drow.oadr = mnm.oadr;
	drow.oflag = mnm.flag;
	placeLabel(comp, drow);
	if (drow.ispc) {
		if (mnm.mem) {
			if (mnm.flag & OF_MWORD) {
				drow.info = QString::number(mnm.mop, 16).toUpper().rightJustified(4, '0');
			} else {
				drow.info = QString::number(mnm.mop & 0xff, 16).toUpper().rightJustified(2, '0');
			}
		} else if (mnm.cond && mnm.met && (drow.oadr >= 0)) {
			if (drow.adr < drow.oadr) {
				drow.icon = QString(":/images/arrdn.png");
			} else if (drow.adr > drow.oadr) {
				drow.icon = QString(":/images/arrup.png");
			} else {
				drow.icon = QString(":/images/redCircle.png");
			}
		}
	}
	return mnm.len;
}

int dasmSome(Computer* comp, unsigned short adr, dasmData& drow) {
	int clen = 0;
	drow.adr = adr;
	drow.flag = getBrk(comp, drow.adr);
	drow.oflag = 0;
	drow.oadr = -1;
	switch(drow.flag & 0xf0) {
		case DBG_VIEW_WORD: clen = dasmWord(comp, adr, drow); break;
		case DBG_VIEW_ADDR: clen = dasmAddr(comp, adr, drow); break;
		case DBG_VIEW_TEXT: clen = dasmText(comp, adr, drow); break;
		case DBG_VIEW_CODE:
		case DBG_VIEW_EXEC:
			clen = dasmCode(comp, adr, drow);
			break;
		default: clen = dasmByte(comp, adr, drow); break;		// DBG_VIEW_BYTE
	}
	return clen;
}

QList<dasmData> getDisasm(Computer* comp, unsigned short& adr) {
	QList<dasmData> list;
	dasmData drow;
	unsigned short oadr = adr;
	drow.adr = adr;
	drow.oflag = 0;
	drow.ispc = 0;
	drow.issel = 0;
	drow.islab = 0;
	drow.isequ = 0;
	drow.info.clear();
	drow.icon.clear();
	int clen = 0;
	// 0:adr
	QString lab;
	xAdr xadr;
	switch (mode) {
		case XVIEW_RAM:
			xadr.type = MEM_RAM;
			xadr.bank = page;
			xadr.adr = adr & 0x3fff;
			xadr.abs = xadr.adr | (page << 14);
			drow.flag = comp->brkRamMap[xadr.abs];
			break;
		case XVIEW_ROM:
			xadr.type = MEM_ROM;
			xadr.bank = page;
			xadr.adr = adr & 0x3fff;
			xadr.abs = xadr.adr | (page << 14);
			drow.flag = comp->brkRomMap[xadr.abs];
			break;
		default:
			xadr = memGetXAdr(comp->mem, adr);
			drow.flag = getBrk(comp, drow.adr);
			drow.ispc = (adr == comp->cpu->pc) ? 1 : 0;
			drow.issel = ((adr >= blockStart) && (adr <= blockEnd)) ? 1 : 0;
			break;
	}
	drow.isbrk = (drow.flag & MEM_BRK_ANY) ? 1 : 0;

	if (conf.dbg.labels) {
		lab = findLabel(xadr.adr, xadr.type, xadr.bank);
	}
	if (lab.isEmpty()) {
		if (conf.dbg.segment || (mode != XVIEW_CPU)) {
			switch(xadr.type) {
				case MEM_RAM: drow.aname = "RAM:"; break;
				case MEM_ROM: drow.aname = "ROM:"; break;
				case MEM_SLOT: drow.aname = "SLT:"; break;
				case MEM_EXT: drow.aname = "EXT:"; break;
				default: drow.aname = "???"; break;
			}
			drow.aname.append(QString("%1:%2").arg(gethexbyte((xadr.bank >> 6) & 0xff)).arg(gethexword(xadr.adr & 0x3fff)));
		} else {
			int wid = (comp->hw->base == 8) ? 6 : 4;
			drow.aname = QString::number(xadr.adr, comp->hw->base).toUpper().rightJustified(wid, '0');
		}
	} else {
		drow.islab = 1;
		drow.aname = lab;
	}
	// 2:command / 3:info
	clen = dasmSome(comp, adr, drow);
	// 1:bytes
	drow.bytes.clear();
	while(clen > 0) {
		drow.bytes.append(gethexbyte(dasmrd(adr, comp)));
		adr++;
		clen--;
	}
	list.append(drow);
// add equ's
	clen = adr - oadr;
	while (clen > 1) {
		oadr++;
		clen--;
		lab = findLabel(oadr, -1, -1);
		if (!lab.isEmpty()) {
			drow.islab = 1;
			drow.isequ = 1;
			drow.adr = oadr;
			drow.aname = lab;
			drow.command = QString("EQU $-%0").arg(clen);
			list.append(drow);
		}
	}

	return list;
}

int xDisasmModel::fill() {
	dasmData drow;
	QList<dasmData> list;
	int row;
	unsigned short adr = disasmAdr & 0xffff;
	int res = 0;
	QString lab;
	dasm.clear();
	for(row = 0; row < rowCount(); row++) {
		list = getDisasm(*cptr, adr);
		foreach(drow, list) {
			dasm.append(drow);
			if (drow.islab && !drow.isequ) {
				drow.islab = 0;
				drow.aname = gethexword(drow.adr);
				dasm.append(drow);
			}
		}
		res |= drow.ispc;
	}
	return res;
}

int xDisasmModel::update() {
	int res = fill();
	int i;
	xMnem mnm = cpuDisasm((*cptr)->cpu, (*cptr)->cpu->pc, NULL, dasmrd, *cptr);
	if (mnm.cond && mnm.met) {
		for (i = 0; i < dasm.size(); i++) {
			if ((dasm[i].adr == mnm.oadr) && (mnm.oadr != (*cptr)->cpu->pc)) {
				dasm[i].icon = QString(":/images/arrleft.png");
			}
		}
	}
	emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
	return res;
}

Qt::ItemFlags xDisasmModel::flags(const QModelIndex& idx) const {
	Qt::ItemFlags res = QAbstractItemModel::flags(idx);
	if ((idx.column() < 3) && !((idx.column() == 2) && dasm[idx.row()].isequ)) {
		res |= Qt::ItemIsEditable;
	}
	return res;
}

unsigned short getPrevAdr(Computer* comp, unsigned short adr) {
	dasmData drow;
	int i;
	for(i = 16; i > 0; i--) {
		if (dasmSome(comp, (adr - i) & 0xffff, drow) == i) {
			adr = adr - i;
			break;
		}
	}
	if (i == 0)
		adr--;
	return adr;
}

unsigned short adr_of_reg(CPU* cpu, bool* flag, QString nam) {
	xRegBunch bch = cpuGetRegs(cpu);
	int i = 0;
	nam = nam.toUpper();
	while ((bch.regs[i].id != REG_NONE) && (QString(bch.regs[i].name).toUpper() != nam))
		i++;
	if (bch.regs[i].id == REG_NONE) {
		*flag = false;
		return 0;
	} else {
		*flag = true;
		return bch.regs[i].value;
	}
}

int str_to_adr(Computer* comp, QString str) {
	bool flag;
	int adr;
	if (str.startsWith(".")) {				// .REG : name of CPU register
		adr = adr_of_reg(comp->cpu, &flag, str.mid(1));
	} else if (str.startsWith("#")) {			// #nnnn : hex adr
		str.replace(0, 1, "0x");
		adr = str.toInt(&flag, 16);
	} else if (str.startsWith("0x")) {			// 0xnnnn : hex adr
		adr = str.toInt(&flag, 16);
	} else if (conf.labels.contains(str)) {			// NAME : label name
		adr = conf.labels[str].adr;
		flag = true;
	} else {						// other : adr in base of comp hardware (16 | 8)
		adr = str.toInt(&flag, comp->hw->base);
	}
	if (!flag) adr = -1;
	return adr;
}

int asmAddr(Computer* comp, QVariant val, xAdr xadr) {
	QString lab;
	QString str = val.toString();
	int adr = str_to_adr(comp, str);
	if (adr < 0) {
		lab = findLabel(xadr.adr, xadr.type, xadr.bank);
		if (!lab.isEmpty())
			conf.labels.remove(lab);
		if (!str.contains(" "))
			conf.labels[str] = xadr;
	}
	return adr;
}

bool xDisasmModel::setData(const QModelIndex& cidx, const QVariant& val, int role) {
	if (!cidx.isValid()) return false;
	if (role != Qt::EditRole) return false;
	int row = cidx.row();
	int col = cidx.column();
	if ((row < 0) || (row >= rowCount())) return false;
	if ((col < 0) || (col >= columnCount())) return false;
	bool flag;
	unsigned char cbyte;
	char buf[256];
	unsigned char* ptr;
	int idx;
	int len;
	QString str;
	// QString lab;
	QStringList lst;
	xAdr xadr;
	int adr = dasm[row].adr;
	switch(col) {
		case 0:
			str = val.toString();
			if (str.startsWith(".")) {
				idx = adr_of_reg((*cptr)->cpu, &flag, str.mid(1));
				if (!flag)
					idx = -1;
			} else {
				xadr = memGetXAdr((*cptr)->mem, dasm[row].adr);
				idx = asmAddr(*cptr, val, xadr);
			}
			if (idx >= 0) {
				while (row > 0) {
					idx = getPrevAdr(*cptr, idx & 0xffff);
					row--;
				}
				disasmAdr = idx & 0xffff;
			}
			emit s_adrch();
			break;
		case 1:	// bytes
			str = val.toString();
			while(!str.isEmpty()) {
				cbyte = str.left(2).toInt(&flag,16) & 0xff;
				if (flag)
					dasmwr(*cptr, adr & 0xffff, cbyte);
				adr++;
				str.remove(0, 2);
			}
			emit rqRefill();
			break;
		case 2:	// command
			ptr = getBrkPtr(*cptr, adr & 0xffff);
			str = val.toString();
			if (!str.startsWith("db \"", Qt::CaseInsensitive))		// #NUM -> 0xNUM if not db "..."
				str.replace("#", "0x");
			if (str.startsWith("db ", Qt::CaseInsensitive)) {		// db
				if ((str.at(3) == '"') && str.endsWith("\"")) {		// db "text"
					str = str.mid(4, str.size() - 5);
					idx = 0;
					cbyte = str.at(idx).cell();
					while ((idx < 250) && (idx < str.size()) && (cbyte > 31) && (cbyte < 128)) {
						buf[idx] = cbyte;
						*ptr &= 0x0f;
						*ptr |= DBG_VIEW_TEXT;
						ptr++;
						idx++;
						cbyte = str.at(idx).cell();
					}
					len = idx;
				} else {						// db n
					str = str.mid(3);
					idx = str.toInt(&flag, 0);
					if (flag) {
						len = 1;
						buf[0] = idx & 0xff;
						*ptr &= 0x0f;
						*ptr |= DBG_VIEW_BYTE;
					} else {
						len = 0;
					}
				}
			} else if (str.startsWith("dw ", Qt::CaseInsensitive)) {	// word/addr
				str = str.mid(3);
				if (conf.labels.contains(str)) {				// check label
					idx = conf.labels[str].adr;
					*ptr &= 0x0f;
					*ptr |= DBG_VIEW_ADDR;
					ptr++;
					*ptr &= 0x0f;
					*ptr |= DBG_VIEW_ADDR;
					len = 2;
				} else {
					idx = str.toInt(&flag, 0);
					if (flag) {
						len = 2;
						*ptr &= 0x0f;
						*ptr |= DBG_VIEW_WORD;
						ptr++;
						*ptr &= 0x0f;
						*ptr |= DBG_VIEW_WORD;
					} else {
						len = 0;
					}
				}
				if (len > 0) {
					buf[0] = idx & 0xff;
					buf[1] = (idx >> 8) & 0xff;
				}
			} else {			// code
				// TODO: replace label name
				len = cpuAsm((*cptr)->cpu, str.toLocal8Bit().data(), buf, adr);
				if (len > 0) {
					for(idx = 0; idx < len; idx++) {
						*ptr &= 0x0f;
						*ptr |= DBG_VIEW_EXEC;
					}
				}
			}
			idx = 0;
			while (idx < len) {
				dasmwr(*cptr, (adr + idx) & 0xffff, buf[idx]);
				idx++;
			}
			emit rqRefill();
			break;
	}
	update();
	return true;
}

// TABLE

xDisasmTable::xDisasmTable(QWidget* p):QTableView(p) {
	cptr = NULL;
	model = new xDisasmModel();
	setModel(model);
	connect(model, SIGNAL(s_adrch()), this, SLOT(updContent()));
	connect(model, SIGNAL(rqRefill()), this, SIGNAL(rqRefill()));
}

QVariant xDisasmTable::getData(int row, int col, int role) {
	return model->data(model->index(row, col), role);
}

int xDisasmTable::rows() {
	return model->rowCount();
}

void xDisasmTable::setComp(Computer** comp) {
	cptr = comp;
	model->cptr = comp;
}

void xDisasmTable::setMode(int md, int pg) {
	mode = md;
	page = pg;
	updContent();
}

int xDisasmTable::updContent() {
	int res = model->update();
	clearSpans();
	for (int i = 0; i < model->dasm.size(); i++) {
		if (model->dasm[i].isequ) {
			setSpan(i, 0, 1, 2);
			setSpan(i, 2, 1, 2);
		} else if (model->dasm[i].islab) {
			setSpan(i, 0, 1, model->columnCount());
		} else if (model->dasm[i].icon.isEmpty() && model->dasm[i].info.isEmpty()) {
			setSpan(i, 2, 1, model->columnCount() - 2);
		}
	}
	return res;
}

void xDisasmTable::keyPressEvent(QKeyEvent* ev) {
	QModelIndex idx = currentIndex();
	int bpt = MEM_BRK_FETCH;
	int bpr = BRK_MEMCELL;
	switch (ev->key()) {
		case Qt::Key_Up:
			if ((ev->modifiers() & Qt::ControlModifier) || (idx.row() == 0)) {
				scrolUp(ev->modifiers());
			} else {
				QTableView::keyPressEvent(ev);
			}
			ev->ignore();
			break;
		case Qt::Key_Down:
			if ((ev->modifiers() & Qt::ControlModifier) || (idx.row() == model->rowCount() - 1)) {
				scrolDn(ev->modifiers());
			} else {
				QTableView::keyPressEvent(ev);
			}
			ev->ignore();
			break;
		case Qt::Key_Home:
			if (mode != XVIEW_CPU) break;
			if (!cptr) break;
			disasmAdr = (*cptr)->cpu->pc;
			updContent();
			ev->ignore();
			break;
		case Qt::Key_End:
			if (mode != XVIEW_CPU) break;
			if (!cptr) break;
			(*cptr)->cpu->pc = getData(idx.row(), 0, Qt::UserRole).toInt() & 0xffff;
			emit rqRefillAll();
			ev->ignore();
			break;
		case Qt::Key_Space:
		case Qt::Key_F2:
			if (ev->modifiers() & Qt::AltModifier) {
				bpr = BRK_CPUADR;
				bpt = MEM_BRK_RD;
			} else if (ev->modifiers() & Qt::ControlModifier) {
				bpr = BRK_CPUADR;
				bpt = MEM_BRK_WR;
			} else if (ev->modifiers() & Qt::ShiftModifier) {
				bpt = MEM_BRK_FETCH;
			} else {
				bpr = BRK_CPUADR;
				bpt = MEM_BRK_FETCH;
			}
			brkXor(bpr, bpt, getData(idx.row(), 0, Qt::UserRole).toInt(), -1, 1);
			emit rqRefill();
			ev->ignore();
			break;
		case Qt::Key_Return:
			edit(currentIndex());
			ev->ignore();
			break;
		default:
			QTableView::keyPressEvent(ev);
			break;
	}
}

void xDisasmTable::mousePressEvent(QMouseEvent* ev) {
	int row = rowAt(ev->pos().y());
	if ((row < 0) || (row >= model->rowCount())) return;
	int adr = getData(row, 0, Qt::UserRole).toInt();
	switch (ev->button()) {
		case Qt::MiddleButton:
			blockStart = -1;
			blockEnd = -1;
			emit rqRefill();
			ev->ignore();
			break;
		case Qt::LeftButton:
			if (mode != XVIEW_CPU) {
				QTableView::mousePressEvent(ev);
			} else if (ev->modifiers() & Qt::ControlModifier) {
				blockStart = adr;
				if (blockEnd < blockStart) blockEnd = blockStart;
				emit rqRefill();
				ev->ignore();
			} else if (ev->modifiers() & Qt::ShiftModifier) {
				blockEnd = adr;
				if (blockStart > blockEnd) blockStart = blockEnd;
				if (blockStart < 0) blockStart = 0;
				emit rqRefill();
				ev->ignore();
			} else {
				markAdr = adr;
				QTableView::mousePressEvent(ev);
			}
			break;
		default:
			QTableView::mousePressEvent(ev);
			break;
	}
}

void xDisasmTable::mouseReleaseEvent(QMouseEvent* ev) {
	if (ev->button() == Qt::LeftButton)
		markAdr = -1;
}

void xDisasmTable::mouseMoveEvent(QMouseEvent* ev) {
	if (mode != XVIEW_CPU) return;
	int row = rowAt(ev->pos().y());
	if ((row < 0) || (row >= model->rowCount())) return;
	int adr = model->data(model->index(row, 0), Qt::UserRole).toInt();	// item(row,0)->data(Qt::UserRole).toInt();
	if ((ev->modifiers() == Qt::NoModifier) && (ev->buttons() & Qt::LeftButton) && (adr != blockStart) && (adr != blockEnd) && (markAdr >= 0)) {
		if (adr < blockStart) {
			blockStart = adr;
			blockEnd = markAdr;
		} else {
			blockStart = markAdr;
			blockEnd = adr;
		}
		emit rqRefill();
	}
	QTableView::mouseMoveEvent(ev);
}

void xDisasmTable::scrolDn(Qt::KeyboardModifiers mod) {
	int i = 1;
	if (mod & Qt::ControlModifier) {
		disasmAdr++;
	} else {
		while (disasmAdr == (getData(i, 0, Qt::UserRole).toInt() & 0xffff))
			i++;
		disasmAdr = getData(i, 0, Qt::UserRole).toInt() & 0xffff;
	}
	updContent();
}

void xDisasmTable::scrolUp(Qt::KeyboardModifiers mod) {
	if (mod & Qt::ControlModifier) {
		disasmAdr--;
	} else {
		disasmAdr = getPrevAdr(*cptr, disasmAdr);
	}
	updContent();
}

void xDisasmTable::wheelEvent(QWheelEvent* ev) {
	if (ev->delta() < 0) {
		scrolDn(ev->modifiers());
	} else if (ev->delta() > 0) {
		scrolUp(ev->modifiers());
	}
}
