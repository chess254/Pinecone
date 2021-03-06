#include "../../h/Action.h"
#include "../../h/ErrorHandler.h"
#include "../../h/StackFrame.h"
#include "../../h/CppProgram.h"

class VarGetAction: public ActionData
{
public:
	
	VarGetAction(size_t in, void ** stackPtrPtrIn, Type typeIn, string idIn):
		ActionData(typeIn, Void, Void)
	{
		offset=in;
		stackPtrPtr=stackPtrPtrIn;
		nameHint=idIn;
		
		setDescription("get " + typeIn->getString() + " '" + idIn + "'");
		
	}
	
	void* execute(void* inLeft, void* inRight)
	{
		if (!(*stackPtrPtr))
		{
			throw PineconeError("something fucked up big time. VarGetAction::execute called while stack pointer is still null", RUNTIME_ERROR);
		}
		
		void* out=malloc(returnType->getSize());
		memcpy(out, (char*)(*stackPtrPtr)+offset, returnType->getSize());
		return out;
	}
	
	void addToProg(Action inLeft, Action inRight, CppProgram* prog)
	{
		if ((nameHint=="me" || nameHint=="in") && getReturnType()->getType()==TypeBase::TUPLE)
		{
			prog->code(prog->getTypeCode(getReturnType()));
			prog->pushExpr();
				bool isFirst=true;
				for (auto i: *getReturnType()->getAllSubTypes())
				{
					if (!isFirst)
						prog->code(", ");
					isFirst=false;
					prog->name(i.name);
				}
			prog->popExpr();
		}
		else
		{
			prog->declareVar(nameHint, getReturnType());
			prog->name(nameHint);
		}
	}
	
private:
	
	void ** stackPtrPtr;
	size_t offset;
};

class VarSetAction: public ActionData
{
public:
	
	VarSetAction(size_t in, void ** stackPtrPtrIn, Type typeIn, string idIn):
		ActionData(typeIn, Void, typeIn)
	{
		offset=in;
		stackPtrPtr=stackPtrPtrIn;
		nameHint=idIn;
		
		setDescription("set " + typeIn->getString() + " '" + idIn + "'");
	}
	
	void* execute(void* left, void* right)
	{
		if (!(*stackPtrPtr))
		{
			throw PineconeError("something fucked up big time. VarSetAction::execute called while stack pointer is still null", RUNTIME_ERROR);
		}
		
		//copy data on to the stack location of the var
		memcpy((char*)(*stackPtrPtr)+offset, right, inRightType->getSize());
		
		//return a new copy of the data
		void* out=malloc(returnType->getSize());
		memcpy(out, (char*)(*stackPtrPtr)+offset, inRightType->getSize());
		return out;
	}
	
	void addToProg(Action inLeft, Action inRight, CppProgram* prog)
	{
		if ((nameHint=="me" || nameHint=="in") && getReturnType()->getType()==TypeBase::TUPLE)
		{
			if (prog->getExprLevel()>0)
			{
				throw PineconeError("can not set 'in' or 'me' inside expression in C++ because of some really dumb reason", INTERNAL_ERROR);
			}
			
			prog->pushBlock();
				prog->code(prog->getTypeCode(inRight->getReturnType()));
				prog->code(" tmp = ");
				prog->pushExpr();
					inRight->addToProg(prog);
				prog->popExpr();
				prog->endln();
				auto subs=*getReturnType()->getAllSubTypes();
				for (int i=0; i<(int)subs.size(); i++)
				{
					prog->name(subs[i].name);
					prog->code(" = ");
					prog->code("tmp."+(*inRight->getReturnType()->getAllSubTypes())[i].name);
					//prog->pushExpr();
						//getElemFromTupleAction(inRight->getReturnType(), (*inRight->getReturnType()->getAllSubTypes())[i].name)->addToProg(var, voidAction, prog);
					//prog->popExpr();
					prog->endln();
				}
			prog->popBlock();
		}
		else
		{
			prog->declareVar(nameHint, getInRightType());
			prog->name(nameHint);
			prog->code(" = ");
			prog->pushExpr();
			inRight->addToProg(prog);
			prog->popExpr();
			//if (prog->getExprLevel()==0)
			//	prog->endln();
		}
	}
	
private:
	
	void ** stackPtrPtr;
	size_t offset;
};

class ConstGetAction: public ActionData
{
public:
	
	ConstGetAction(void* in, Type typeIn, string textIn):
		ActionData(typeIn, Void, Void)
	{
		data=malloc(returnType->getSize());
		memcpy(data, in, returnType->getSize());
		
		setDescription(textIn);// + " (" + typeIn.toString() + " literal)");
	}

	~ConstGetAction()
	{
		free(data);
	}
	
	void* execute(void* inLeft, void* inRight)
	{
		void* out=malloc(returnType->getSize());
		memcpy(out, data, returnType->getSize());
		return out;
	}
	
	void addToProg(Action inLeft, Action inRight, CppProgram* prog)
	{
		if (getReturnType()!=String)
		{
			prog->code(getReturnType()->getCppLiteral(data, prog));
		}
		else // getReturnType()==String
		{
			//prog->code(prog->getTypeCode(getReturnType()));
			addToProgPnStr(prog);
			prog->name("$pnStr");
			prog->pushExpr();
				auto sizeInfo=getReturnType()->getSubType("_size");
				auto dataInfo=getReturnType()->getSubType("_data");
				if (sizeInfo.type!=Int || dataInfo.type!=Byte->getPtr())
				{
					throw PineconeError("ConstGetAction::addToProg failed to access string properties", INTERNAL_ERROR);
				}
				//prog->code(Int->getCppLiteral((char*)data+sizeInfo.offset, prog));
				//prog->code(", (unsigned char*)\"");
				prog->code("\"");
				int len=*(int*)((char*)data+sizeInfo.offset);
				for (int i=0; i<len; i++)
				{
					char c=*((*(char**)((char*)data+dataInfo.offset))+i);
					
					if (c=='"')
					{
						prog->code("\\\"");
					}
					else if (c=='\\')
					{
						prog->code("\\\\");
					}
					else if (c>=32 && c<=126)
					{
						prog->code(string()+c);
					}
					else if (c==0)
					{
						prog->code("\0");
					}
					else if (c=='\n')
					{
						prog->code("\\n");
					}
					else
					{
						prog->code("\\x");
						char buf[8];
						sprintf(buf, "%02X", c);
						prog->code(string()+buf);
					}
					
				}
				prog->code("\"");
			prog->popExpr();
		}
	}
	
private:
	
	void* data;
};

Action varGetAction(size_t in, Type typeIn, string textIn)
{
	return Action(new VarGetAction(in, &stackPtr, typeIn, textIn));
}

Action varSetAction(size_t in, Type typeIn, string varNameIn)
{
	return Action(new VarSetAction(in, &stackPtr, typeIn, varNameIn));
}

Action globalGetAction(size_t in, Type typeIn, string textIn)
{
	return Action(new VarGetAction(in, &globalFramePtr, typeIn, textIn));
}

Action globalSetAction(size_t in, Type typeIn, string textIn)
{
	return Action(new VarSetAction(in, &globalFramePtr, typeIn, textIn));
}

Action constGetAction(void* in, Type typeIn, string textIn)
{
	return Action(new ConstGetAction(in, typeIn, textIn));
}
