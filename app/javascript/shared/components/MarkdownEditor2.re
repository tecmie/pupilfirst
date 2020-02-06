[@bs.config {jsx: 3}];

let str = React.string;

type fullscreenMode = [ | `Editor | `Preview | `Split];

type windowedMode = [ | `Editor | `Preview];

type mode =
  | Fullscreen(fullscreenMode)
  | Windowed(windowedMode);

type selection = (selectionStart, selectionEnd)
and selectionStart = int
and selectionEnd = int;

type uploadState =
  | Uploading
  | ReadyToUpload(uploadError)
and uploadError = option(string);

type state = {
  id: string,
  mode,
  selection,
  uploadState,
};

type action =
  | ClickPreview
  | ClickSplit
  | ClickFullscreen
  | SetSelection(selection)
  | BumpSelection(int)
  | PressEscapeKey
  | SetUploadError(uploadError)
  | SetUploading
  | FinishUploading;

let reducer = (state, action) =>
  switch (action) {
  | ClickPreview =>
    let mode =
      switch (state.mode) {
      | Windowed(`Preview) => Windowed(`Editor)
      | Windowed(`Editor) => Windowed(`Preview)
      | Fullscreen(`Editor)
      | Fullscreen(`Split) => Fullscreen(`Preview)
      | Fullscreen(`Preview) => Fullscreen(`Editor)
      };
    {...state, mode};
  | ClickSplit =>
    let mode =
      switch (state.mode) {
      | Windowed(_) => Fullscreen(`Split)
      | Fullscreen(`Editor)
      | Fullscreen(`Preview) => Fullscreen(`Split)
      | Fullscreen(`Split) => Fullscreen(`Editor)
      };
    {...state, mode};
  | ClickFullscreen =>
    let mode =
      switch (state.mode) {
      | Windowed(`Editor) => Fullscreen(`Editor)
      | Windowed(`Preview) => Fullscreen(`Preview)
      | Fullscreen(`Editor) => Windowed(`Editor)
      | Fullscreen(`Preview) => Windowed(`Preview)
      | Fullscreen(`Split) => Windowed(`Editor)
      };
    {...state, mode};
  | SetSelection(selection) => {...state, selection}
  | BumpSelection(offset) =>
    let (selectionStart, selectionEnd) = state.selection;
    {...state, selection: (selectionStart + offset, selectionEnd + offset)};
  | PressEscapeKey =>
    let mode =
      switch (state.mode) {
      | Fullscreen(`Editor) => Windowed(`Editor)
      | Windowed(`Preview)
      | Fullscreen(`Preview) => Windowed(`Preview)
      | Windowed(`Editor)
      | Fullscreen(`Split) => Windowed(`Editor)
      };
    {...state, mode};
  | SetUploadError(error) => {...state, uploadState: ReadyToUpload(error)}
  | SetUploading => {...state, uploadState: Uploading}
  | FinishUploading => {...state, uploadState: ReadyToUpload(None)}
  };

let computeInitialState = ((value, textareaId, mode)) => {
  let id =
    switch (textareaId) {
    | Some(id) => id
    | None => DateTime.randomId()
    };

  let length = value |> String.length;

  {id, mode, selection: (length, length), uploadState: ReadyToUpload(None)};
};

let containerClasses = mode =>
  switch (mode) {
  | Windowed(_) => ""
  | Fullscreen(_) => "bg-white fixed z-50 top-0 left-0 h-screen w-screen flex flex-col"
  };

let modeIcon = (desiredMode, currentMode) => {
  let icon =
    switch (desiredMode, currentMode) {
    | (
        `Preview,
        Windowed(`Editor) | Fullscreen(`Editor) | Fullscreen(`Split),
      ) => "fas fa-eye"
    | (`Preview, Windowed(`Preview) | Fullscreen(`Preview)) => "fas fa-pen-nib"
    | (`Split, Windowed(_) | Fullscreen(`Editor) | Fullscreen(`Preview)) => "fas fa-columns"
    | (`Split, Fullscreen(`Split)) => "far fa-window-maximize"
    | (`Fullscreen, Windowed(_)) => "fas fa-expand"
    | (`Fullscreen, Fullscreen(_)) => "fas fa-compress"
    };

  <FaIcon classes={"fa-fw " ++ icon} />;
};

let onClickFullscreen = (state, send, _event) => {
  switch (state.mode) {
  | Windowed(_) => TextareaAutosize.destroy(state.id)
  | Fullscreen(_) => () // Do nothing here. We'll fix this in an effect.
  };

  send(ClickFullscreen);
};

let onClickPreview = (state, send, _event) => {
  switch (state.mode) {
  | Windowed(`Editor) => TextareaAutosize.destroy(state.id)
  | Windowed(`Preview)
  | Fullscreen(_) => () // Do nothing here. We'll fix this in an effect.
  };

  send(ClickPreview);
};

let onClickSplit = (state, send, _event) => {
  switch (state.mode) {
  | Windowed(_) => TextareaAutosize.destroy(state.id)
  | Fullscreen(_) => () // This should have no effect on textarea autosizing in full-screen mode.
  };

  send(ClickSplit);
};

let insertAt = (textToInsert, position, sourceText) => {
  let head = sourceText->String.sub(0, position);
  let tail =
    sourceText->String.sub(
      position,
      (sourceText |> String.length) - position,
    );

  head ++ textToInsert ++ tail;
};

let wrapWith = (wrapper, selectionStart, selectionEnd, sourceText) => {
  let head = sourceText->String.sub(0, selectionStart);
  let selection =
    sourceText->String.sub(selectionStart, selectionEnd - selectionStart);
  let tail =
    sourceText->String.sub(
      selectionEnd,
      (sourceText |> String.length) - selectionEnd,
    );

  head ++ wrapper ++ selection ++ wrapper ++ tail;
};

/**
  * After changing the Markdown using any of the controls or key commands, the
  * textarea element will need to be manually "synced" in two ways:
  *
  * 1. The autosize update function needs to be called to let it know that we
  *    have changed the value of the textare from the outside.
  * 2. The cursor position will have jumped to the end of the text-area because
  *    of the manual change of value of the controlled component; we'll need to
  *    manually set the cursor position after the component has had a change to
  *    re-render.
  *
  * This function is making an assumption that re-render can happen in 25ms.
  * The need for these manual adjustments can be visibly seen by increasing the
  * renderDelay to something like 1000ms.
 **/
let updateTextareaAfterDelay = (state, cursorPosition) => {
  let renderDelay = 25; //ms

  switch (state.mode) {
  | Windowed(_) =>
    Js.Global.setTimeout(
      () => TextareaAutosize.update(state.id),
      renderDelay,
    )
    |> ignore
  | Fullscreen(_) => () // Autosizing is turned off in full-screen mode.
  };

  Webapi.Dom.(
    switch (document |> Document.getElementById(state.id)) {
    | Some(element) =>
      Js.Global.setTimeout(
        () =>
          element
          |> DomUtils.Element.unsafeToHtmlInputElement
          |> HtmlInputElement.setSelectionRange(
               cursorPosition,
               cursorPosition,
             ),
        renderDelay,
      )
      |> ignore
    | None => () // Avoid messing with the DOM if the textarea can't be found.
    }
  );
};

let finalizeChange = (~oldValue, ~newValue, ~state, ~send, ~onChange) => {
  let offset = (newValue |> String.length) - (oldValue |> String.length);
  let (_, selectionEnd) = state.selection;

  // The cursor needs to be bumped to account for changed value.
  send(BumpSelection(offset / 2));

  // Report the modified value to the parent.
  onChange(newValue);

  // Update the textarea after state changes are applied. Read more in function's documentation.
  updateTextareaAfterDelay(state, selectionEnd + offset);
};

type phraseModifer =
  | Bold
  | Italic
  | Strikethrough;

let insertAndWrapper = phraseModifer =>
  switch (phraseModifer) {
  | Bold => ("**bold**", "**")
  | Italic => ("*italics*", "*")
  | Strikethrough => ("~~strikethrough~~", "~~")
  };

let modifyPhrase = (oldValue, state, send, onChange, phraseModifer) => {
  let (selectionStart, selectionEnd) = state.selection;
  let (insert, wrapper) = phraseModifer |> insertAndWrapper;

  let newValue =
    if (selectionStart == selectionEnd) {
      oldValue |> insertAt(insert, selectionStart);
    } else {
      oldValue |> wrapWith(wrapper, selectionStart, selectionEnd);
    };

  finalizeChange(~oldValue, ~newValue, ~state, ~send, ~onChange);
};

let controls = (value, state, send, onChange) => {
  let buttonClasses = "border rounded p-1 hover:bg-gray-300 ";
  let {mode} = state;
  let curriedModifyPhrase = modifyPhrase(value, state, send, onChange);

  <div className="bg-gray-100 p-1 flex justify-between">
    {switch (mode) {
     | Windowed(`Preview)
     | Fullscreen(`Preview) => <div />
     | Windowed(`Editor)
     | Fullscreen(`Editor | `Split) =>
       <div>
         <button
           className=buttonClasses onClick={_ => curriedModifyPhrase(Bold)}>
           <FaIcon classes="fas fa-bold fa-fw" />
         </button>
         <button
           className={buttonClasses ++ "ml-2"}
           onClick={_ => curriedModifyPhrase(Italic)}>
           <FaIcon classes="fas fa-italic fa-fw" />
         </button>
         <button
           className={buttonClasses ++ "ml-2"}
           onClick={_ => curriedModifyPhrase(Strikethrough)}>
           <FaIcon classes="fas fa-strikethrough fa-fw" />
         </button>
       </div>
     }}
    <div>
      <button className=buttonClasses onClick={onClickPreview(state, send)}>
        {modeIcon(`Preview, mode)}
      </button>
      <button
        className={buttonClasses ++ "ml-2 hidden md:inline"}
        onClick={onClickSplit(state, send)}>
        {modeIcon(`Split, mode)}
      </button>
      <button
        className={buttonClasses ++ "ml-2 hidden md:inline"}
        onClick={onClickFullscreen(state, send)}>
        {modeIcon(`Fullscreen, mode)}
        {switch (mode) {
         | Fullscreen(_) =>
           <span className="ml-2"> {"Exit full-screen" |> str} </span>
         | Windowed(_) => React.null
         }}
      </button>
    </div>
  </div>;
};

let modeClasses = mode =>
  switch (mode) {
  | Windowed(_) => ""
  | Fullscreen(_) => "flex flex-grow"
  };

let editorContainerClasses = mode =>
  "border "
  ++ (
    switch (mode) {
    | Windowed(`Editor) => ""
    | Windowed(`Preview) => "hidden"
    | Fullscreen(`Editor) => "w-full"
    | Fullscreen(`Preview) => "hidden"
    | Fullscreen(`Split) => "w-1/2"
    }
  );

let previewContainerClasses = mode =>
  switch (mode) {
  | Windowed(`Editor) => "hidden"
  | Windowed(`Preview) => ""
  | Fullscreen(`Editor) => "hidden"
  | Fullscreen(`Preview) => "w-screen mx-auto"
  | Fullscreen(`Split) => "w-1/2 relative"
  };

let previewClasses = mode =>
  switch (mode) {
  | Fullscreen(`Split | `Preview) => "absolute max-h-full overflow-auto w-full px-2 pb-8"
  | Fullscreen(`Editor)
  | Windowed(_) => ""
  };

let focusOnEditor = id => {
  Webapi.Dom.(
    document
    |> Document.getElementById(id)
    |> OptionUtils.flatMap(HtmlElement.ofElement)
    |> OptionUtils.mapWithDefault(element => element |> HtmlElement.focus, ())
  );
};

let handleUploadFileResponse = (oldValue, state, send, onChange, json) => {
  let errors = json |> Json.Decode.(field("errors", array(string)));

  if (errors == [||]) {
    let markdownEmbedCode =
      json |> Json.Decode.(field("markdownEmbedCode", string));

    let insert = "\n" ++ markdownEmbedCode ++ "\n";
    let (_, selectionEnd) = state.selection;
    let newValue = oldValue |> insertAt(insert, selectionEnd);
    finalizeChange(~oldValue, ~newValue, ~state, ~onChange);
    send(FinishUploading);
  } else {
    send(
      SetUploadError(
        Some(
          "Failed to attach file! " ++ (errors |> Js.Array.joinWith(", ")),
        ),
      ),
    );
  };
};

let submitForm = (formId, oldValue, state, send, onChange) => {
  ReactDOMRe._getElementById(formId)
  |> OptionUtils.mapWithDefault(
       element => {
         let formData = DomUtils.FormData.create(element);

         Api.sendFormData(
           "/markdown_attachments/",
           formData,
           handleUploadFileResponse(oldValue, state, send, onChange),
           () =>
           send(
             SetUploadError(
               Some(
                 "An unexpected error occured! Please reload the page before trying again.",
               ),
             ),
           )
         );
       },
       (),
     );
};

let attachFile = (fileFormId, oldValue, state, send, onChange, event) =>
  switch (ReactEvent.Form.target(event)##files) {
  | [||] => ()
  | files =>
    let file = files[0];
    let maxFileSize = 5 * 1024 * 1024;

    let error =
      file##size > maxFileSize
        ? Some("The maximum file size is 5 MB. Please select another file.")
        : None;

    switch (error) {
    | Some(_) => send(SetUploadError(error))
    | None =>
      send(SetUploading);
      submitForm(fileFormId, oldValue, state, send, onChange);
    };
  };

let footer = (oldValue, state, send, onChange) => {
  let {id} = state;
  let fileFormId = id ++ "-file-form";
  let fileInputId = id ++ "-file-input";

  switch (state.mode) {
  | Windowed(`Preview)
  | Fullscreen(`Preview) => React.null
  | Windowed(`Editor)
  | Fullscreen(`Editor | `Split) =>
    <div
      className="bg-gray-100 border-t border-gray-400 border-dashed flex justify-between items-center">
      <form
        className="flex items-center flex-wrap flex-1 text-sm font-semibold hover:bg-gray-200 hover:text-primary-500"
        id=fileFormId>
        <input
          name="authenticity_token"
          type_="hidden"
          value={AuthenticityToken.fromHead()}
        />
        <input
          className="hidden"
          type_="file"
          name="markdown_attachment[file]"
          id=fileInputId
          multiple=false
          onChange={attachFile(fileFormId, oldValue, state, send, onChange)}
        />
        {switch (state.uploadState) {
         | ReadyToUpload(error) =>
           <label
             className="text-xs px-3 py-1 flex-grow cursor-pointer"
             htmlFor=fileInputId>
             {switch (error) {
              | Some(error) =>
                <span className="text-red-500">
                  <FaIcon classes="fas fa-exclamation-triangle mr-2" />
                  {error |> str}
                </span>
              | None =>
                <span>
                  <FaIcon classes="far fa-file-image mr-2" />
                  {"Click here to attach a file." |> str}
                </span>
              }}
           </label>
         | Uploading =>
           <span className="text-xs px-3 py-1 flex-grow cursor-wait">
             <FaIcon classes="fas fa-spinner fa-pulse mr-2" />
             {"Please wait for the file to upload..." |> str}
           </span>
         }}
      </form>
      <a
        href="/help/markdown_editor"
        target="_blank"
        className="flex items-center px-3 py-1 hover:bg-gray-200 hover:text-secondary-500 cursor-pointer">
        <FaIcon classes="fab fa-markdown text-sm" />
        <span className="text-xs ml-1 font-semibold hidden sm:inline">
          {"Need help?" |> str}
        </span>
      </a>
    </div>
  };
};

let textareaClasses = mode => {
  "w-full outline-none font-mono "
  ++ (
    switch (mode) {
    | Windowed(_) => "p-2"
    | Fullscreen(_) => "px-2 pt-4 pb-8 h-full resize-none"
    }
  );
};

let onChangeWrapper = (onChange, event) => {
  let value = ReactEvent.Form.target(event)##value;
  onChange(value);
};

let onSelect = (send, event) => {
  let htmlInputElement =
    ReactEvent.Selection.target(event)
    |> DomUtils.EventTarget.unsafeToHtmlInputElement;

  let selection =
    Webapi.Dom.(
      htmlInputElement |> HtmlInputElement.selectionStart,
      htmlInputElement |> HtmlInputElement.selectionEnd,
    );

  send(SetSelection(selection));
};

let handleEscapeKey = (send, event) =>
  switch (event |> Webapi.Dom.KeyboardEvent.key) {
  | "Escape" => send(PressEscapeKey)
  | _anyOtherKey => ()
  };

let handleKeyboardControls = (value, state, send, onChange, event) => {
  let ctrlKey = Webapi.Dom.KeyboardEvent.ctrlKey;
  let metaKey = Webapi.Dom.KeyboardEvent.metaKey;
  let curriedModifyPhrase = modifyPhrase(value, state, send, onChange);

  switch (event |> Webapi.Dom.KeyboardEvent.key) {
  | "b" when event |> ctrlKey || event |> metaKey => curriedModifyPhrase(Bold)
  | "i" when event |> ctrlKey || event |> metaKey =>
    curriedModifyPhrase(Italic)
  | _anyOtherKey => ()
  };
};

module ScrollSync = {
  open Webapi.Dom;

  /*
   * There's a tiny bit of math involved in correctly mapping the source
   * element's scroll position to the desired scroll of the target element.
   * The source's scrollTop varies from zero to a number that's the difference
   * between its scrollHeight and its offsetHeight; the same applies for the
   * target. This needs to be taken into account when mapping one scroll
   * position to the other.
   */
  let scrollTargetToSource = (~source, ~target, _event) => {
    let sourceScrollTop = source |> Element.scrollTop;
    let sourceOffsetHeight =
      source |> Element.unsafeAsHtmlElement |> HtmlElement.offsetHeight;
    let sourceScrollHeight = source |> Element.scrollHeight;

    let scrollFraction =
      sourceScrollTop
      /. (sourceScrollHeight - sourceOffsetHeight |> float_of_int);

    let maxTargetScrollTop =
      (target |> Element.scrollHeight)
      - (target |> Element.unsafeAsHtmlElement |> HtmlElement.offsetHeight)
      |> float_of_int;

    target->Element.setScrollTop(scrollFraction *. maxTargetScrollTop);
  };
};

[@react.component]
let make =
    (
      ~value,
      ~onChange,
      ~profile,
      ~textareaId=?,
      ~maxLength=1000,
      ~defaultMode=Windowed(`Editor),
    ) => {
  let (state, send) =
    React.useReducerWithMapState(
      reducer,
      (value, textareaId, defaultMode),
      computeInitialState,
    );

  // Reset autosize when switching from full-screen mode.
  React.useEffect1(
    () => {
      switch (state.mode) {
      | Windowed(`Editor) => TextareaAutosize.create(state.id)
      | Windowed(`Preview)
      | Fullscreen(_) => () // Do nothing. This was handled in the click handler.
      };

      Some(() => TextareaAutosize.destroy(state.id));
    },
    [|state.mode|],
  );

  // Use Escape key to close full-screen mode.
  React.useEffect0(() => {
    let curriedHandler = handleEscapeKey(send);
    let documentEventTarget = Webapi.Dom.(document |> Document.asEventTarget);

    documentEventTarget
    |> Webapi.Dom.EventTarget.addKeyDownEventListener(curriedHandler);

    Some(
      () =>
        documentEventTarget
        |> Webapi.Dom.EventTarget.removeKeyDownEventListener(curriedHandler),
    );
  });

  // Handle keyboard shortcuts for Bold and Italics buttons.
  React.useEffect(() => {
    let curriedHandler = handleKeyboardControls(value, state, send, onChange);
    let textareaEventTarget =
      Webapi.Dom.(
        document
        |> Document.getElementById(state.id)
        |> OptionUtils.map(Element.asEventTarget)
      );

    textareaEventTarget
    |> OptionUtils.mapWithDefault(
         Webapi.Dom.EventTarget.addKeyDownEventListener(curriedHandler),
         (),
       );

    Some(
      () =>
        textareaEventTarget
        |> OptionUtils.mapWithDefault(
             Webapi.Dom.EventTarget.removeKeyDownEventListener(
               curriedHandler,
             ),
             (),
           ),
    );
  });

  React.useEffect1(
    () => {
      let textarea =
        Webapi.Dom.(document |> Document.getElementById(state.id));
      let preview =
        Webapi.Dom.(
          document |> Document.getElementById(state.id ++ "-preview")
        );

      switch (textarea, preview) {
      | (Some(textarea), Some(preview)) =>
        let scrollCallback =
          ScrollSync.scrollTargetToSource(~source=textarea, ~target=preview);

        switch (state.mode) {
        | Fullscreen(`Split) =>
          textarea
          |> Webapi.Dom.Element.addEventListener("scroll", scrollCallback);

          Some(
            () =>
              textarea
              |> Webapi.Dom.Element.removeEventListener(
                   "scroll",
                   scrollCallback,
                 ),
          );
        | _anyOtherMode =>
          textarea
          |> Webapi.Dom.Element.removeEventListener("scroll", scrollCallback);
          None;
        };
      | (_, _) => None
      };
    },
    [|state.mode|],
  );

  <div className={containerClasses(state.mode)}>
    {controls(value, state, send, onChange)}
    <div className={modeClasses(state.mode)}>

        <div className={editorContainerClasses(state.mode)}>
          <DisablingCover
            containerClasses="h-full"
            disabled={state.uploadState == Uploading}
            message="Uploading...">
            <textarea
              ariaLabel="Markdown editor"
              rows=4
              maxLength
              onSelect={onSelect(send)}
              onChange={onChangeWrapper(onChange)}
              id={state.id}
              value
              className={textareaClasses(state.mode)}
            />
          </DisablingCover>
        </div>
      </div>
      // <div className={previewContainerClasses(state.mode)}>
      //   <div
      //     id={state.id ++ "-preview"} className={previewClasses(state.mode)}>
      //     <MarkdownBlock
      //       markdown=value
      //       profile
      //       className="max-w-3xl mx-auto"
      //     />
      //   </div>
      // </div>
    {footer(value, state, send, onChange)}
  </div>;
};