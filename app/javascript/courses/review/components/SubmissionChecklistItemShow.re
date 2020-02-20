[@bs.config {jsx: 3}];

module ChecklistItem = SubmissionChecklistItem;

let str = React.string;

let kindIconClasses = result => {
  switch ((result: ChecklistItem.result)) {
  | ShortText(_text) => "if i-short-text-regular md:text-base text-gray-800"
  | LongText(_markdown) => "if i-long-text-regular md:text-base text-gray-800"
  | Link(_link) => "if i-link-regular md:text-base text-gray-800"
  | MultiChoice(_text) => "if i-check-circle-alt-regular md:text-base text-gray-800"
  | Files(_attachments) => "if i-file-regular md:text-base text-gray-800"
  | None => "if i-question-square-regular md:text-base text-gray-800"
  };
};

let showFiles = attachments => {
  <div className="flex flex-wrap">
    {attachments
     |> Array.map(attachment => {
          <a
            key={"file-" ++ (attachment |> ChecklistItem.attachmentUrl)}
            href={attachment |> ChecklistItem.attachmentUrl}
            target="_blank"
            className="mt-2 mr-3 flex items-center border overflow-hidden shadow rounded hover:shadow-md border-primary-400 bg-primary-200 text-primary-500 hover:border-primary-600 hover:text-primary-700">
            <span
              className="flex h-full w-8 justify-center items-center p-2 bg-primary-200">
              <i className="far fa-file" />
            </span>
            <span
              className="course-show-attachments__attachment-title rounded text-xs font-semibold inline-block whitespace-normal truncate w-32 md:w-42 h-full px-3 py-1 leading-loose bg-primary-100">
              {attachment |> ChecklistItem.attachmentName |> str}
            </span>
          </a>
        })
     |> React.array}
  </div>;
};

let showlink = link =>
  <a
    href=link
    target="_blank"
    className="max-w-fc mt-2 mr-3 flex items-center border overflow-hidden shadow rounded hover:shadow-md border-blue-400 bg-blue-200 text-blue-700 hover:border-blue-600 hover:text-blue-800">
    <span
      className="flex h-full w-8 justify-center items-center p-2 bg-blue-200">
      <i className="fas fa-link" />
    </span>
    <span
      className="course-show-attachments__attachment-title rounded text-xs font-semibold inline-block whitespace-normal truncate w-32 md:w-42 h-full px-3 py-1 leading-loose bg-blue-100">
      {link |> str}
    </span>
  </a>;

let statusIcon = (updateChecklistCB, status) => {
  switch (updateChecklistCB, status: ChecklistItem.status) {
  | (None, Passed) => <FaIcon classes="fas fa-check text-green-500 mr-2" />
  | (None, Failed) => <FaIcon classes="fas fa-times text-red-500 mr-2" />
  | (_, _) => React.null
  };
};

let showStatus = status => {
  switch ((status: ChecklistItem.status)) {
  | Passed =>
    <div className="bg-green-200 rounded px-1 py-px text-green-800 text-tiny">
      {"Passed" |> str}
    </div>
  | Failed =>
    <div className="bg-red-200 rounded px-1 py-px text-red-800 text-tiny">
      {"Failed" |> str}
    </div>
  | Pending => React.null
  };
};

let statusButtonSelectedClasses = (status, currentStatus) => {
  "inline-flex items-center cursor-pointer leading-tight px-3 py-2 font-semibold inline-block text-xs relative hover:bg-gray-100 hover:text-gray-700 "
  ++ (
    switch (currentStatus: ChecklistItem.status, status: ChecklistItem.status) {
    | (Passed, Passed) => "bg-green-100 hover:bg-green-100 text-green-800 hover:text-green-800 border-green-500 z-10"
    | (Failed, Failed) => "bg-red-100 hover:bg-red-100 text-red-800 hover:text-red-800 border-red-500 z-10"
    | (_, _) => "bg-white"
    }
  );
};

let statusButtons = (index, status, callback, checklist) =>
  <div className="mt-2">
    <div
      onClick={_ => callback(checklist |> ChecklistItem.makePassed(index))}
      className={
        "border border-gray-500 rounded-l "
        ++ statusButtonSelectedClasses(ChecklistItem.Passed, status)
      }>
      <PfIcon className="if i-check-light if-fw mr-2" />
      {"Done" |> str}
    </div>
    <div
      onClick={_ => callback(checklist |> ChecklistItem.makeFailed(index))}
      className={
        "border border-gray-500 rounded-r -ml-px "
        ++ statusButtonSelectedClasses(ChecklistItem.Failed, status)
      }>
      <PfIcon className="if i-times-light if-fw mr-2" />
      {"Not Done" |> str}
    </div>
    {switch ((status: ChecklistItem.status)) {
     | Pending => React.null
     | Passed
     | Failed =>
       <div
         onClick={_ =>
           callback(checklist |> ChecklistItem.makePending(index))
         }
         className={statusButtonSelectedClasses(
           ChecklistItem.Pending,
           status,
         )}>
         <i className="fas fa-redo" />
       </div>
     }}
  </div>;

let computeShowResult = (checklistItem, updateChecklistCB) => {
  switch (updateChecklistCB, checklistItem |> ChecklistItem.status) {
  | (Some(_), Pending | Passed | Failed) => true
  | (None, Pending | Failed) => true
  | (None, Passed) => false
  };
};

[@react.component]
let make = (~index, ~checklistItem, ~updateChecklistCB, ~checklist) => {
  let (showResult, setShowResult) =
    React.useState(() => computeShowResult(checklistItem, updateChecklistCB));
  let status = checklistItem |> ChecklistItem.status;

  <div className="py-4">
    <div className="text-sm font-semibold flex items-center justify-between">
      <div className="flex">
        {statusIcon(updateChecklistCB, status)}
        <span>
          <PfIcon
            className={kindIconClasses(checklistItem |> ChecklistItem.result)}
          />
        </span>
        <span className="ml-2 md:ml-3 tracking-wide">
          {checklistItem |> ChecklistItem.title |> str}
        </span>
      </div>
      <div className="inline-block">
        {showResult
           ? showStatus(status)
           : <button onClick={_ => setShowResult(_ => true)}>
               <i className="fas fa-chevron-circle-down" />
             </button>}
      </div>
    </div>
    {showResult
       ? <div className="ml-6 md:ml-7 pt-2 ">
           <div>
             {switch (checklistItem |> ChecklistItem.result) {
              | ShortText(text) => <div> {text |> str} </div>
              | LongText(markdown) =>
                <MarkdownBlock profile=Markdown.Permissive markdown />
              | Link(link) => showlink(link)
              | MultiChoice(text) => <div> {text |> str} </div>
              | Files(attachments) => showFiles(attachments)
              | None => <div> {"Handle Empty" |> str} </div>
              }}
           </div>
           {switch (updateChecklistCB) {
            | Some(callback) =>
              statusButtons(index, status, callback, checklist)
            | None => React.null
            }}
         </div>
       : React.null}
  </div>;
};